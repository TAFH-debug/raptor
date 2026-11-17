#include <network_io.h>
#include <log.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace raptor {

namespace {

constexpr std::array<unsigned char, 3> RAPTOR_HEADER = {0x23, 0x41, 0x32};
constexpr std::array<unsigned char, 3> RAPTOR_VERSION = {0x00, 0x00, 0x01};

constexpr std::size_t MAX_REASON = 1024;

void encode_len(std::uint32_t len, char* buf) {
    for (std::size_t i = 0; i < kLenWidth; ++i)
        buf[i] = static_cast<char>((len >> (8 * i)) & 0xFF);
}

std::uint32_t decode_len(const unsigned char* buf) {
    std::uint32_t len = 0;
    for (std::size_t i = 0; i < kLenWidth; ++i)
        len |= std::uint32_t{buf[i]} << (8 * i);
    return len;
}

template <class WriteBody>
std::string make_frame(const std::string& route, MessageType type, std::uint32_t id, WriteBody&& write_body) {
    std::ostringstream os;
    os.write("\0\0\0\0", kLenWidth);
    write_string(os, route);
    write_int<std::uint8_t>(os, type);
    write_int<std::uint32_t>(os, id);
    write_body(os);

    std::string frame = std::move(os).str();
    const std::size_t payload_size = frame.size() - kLenWidth;
    if (payload_size > MAX_MESSAGE_SIZE)
        throw std::runtime_error("RaptorError: message too large");
    encode_len(static_cast<std::uint32_t>(payload_size), frame.data());
    return frame;
}

std::string make_failure(std::uint32_t id, RpcError code, std::string reason) {
    if (reason.size() > MAX_REASON) reason.resize(MAX_REASON);
    std::replace(reason.begin(), reason.end(), '\0', ' ');
    return make_frame({}, MessageType::FAILURE, id, [&](std::ostream& os) {
        write_int<std::uint8_t>(os, static_cast<std::uint8_t>(code));
        write_string(os, reason);
    });
}

class RpcCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "raptor.rpc"; }

    std::string message(int ev) const override {
        switch (static_cast<RpcError>(ev)) {
            case RpcError::UnknownRoute: return "no handler for this route";
            case RpcError::HandlerFailed: return "remote handler failed";
        }
        return "unknown rpc error";
    }
};

} // namespace

const std::error_category& rpc_category() {
    static const RpcCategory category;
    return category;
}

Connection::Connection(asio::io_context& io) : socket_(asio::make_strand(io)) {}

Connection::pointer Connection::create(asio::io_context& io) {
    return pointer(new Connection(io));
}

std::string Connection::encode_frame(const std::string& route, MessageType type, std::uint32_t id,
                                     const Serializable& body) {
    return make_frame(route, type, id, [&](std::ostream& os) { body.write_binary(os); });
}

std::uint32_t Connection::next_request_id() {
    std::uint32_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) id = next_id_.fetch_add(1, std::memory_order_relaxed); // 0 = "no reply expected"
    return id;
}


void Connection::send_message(std::string route, MessageType type, const Serializable& data, Handler handler) {
    if (type != MessageType::EVENT && type != MessageType::TEXT)
        throw std::invalid_argument("RaptorError: send_message sends EVENT/TEXT only, use invoke() for requests");

    std::string frame = encode_frame(route, type, 0, data);

    asio::post(socket_.get_executor(),
        [self = shared_from_this(), out = Outgoing{std::move(frame), std::move(handler)}]() mutable {
            self->queue_frame(std::move(out));
        });
}

void Connection::start_request(std::uint32_t id, std::string route, Completion complete,
                               std::chrono::milliseconds timeout, std::string frame) {
    asio::post(socket_.get_executor(),
        [self = shared_from_this(), id, route = std::move(route), complete = std::move(complete),
         timeout, frame = std::move(frame)]() mutable {
            if (self->closed_) return complete(asio::error::not_connected, nullptr);

            Pending pending{std::move(route), std::move(complete), nullptr};
            if (timeout.count() > 0) {
                pending.timer = std::make_unique<asio::steady_timer>(self->socket_.get_executor(), timeout);
                pending.timer->async_wait([self, id](std::error_code ec) {
                    if (ec == asio::error::operation_aborted) return;
                    self->finish_request(id, std::make_error_code(std::errc::timed_out), nullptr);
                });
            }
            self->pending_.emplace(id, std::move(pending));
            self->queue_frame(Outgoing{std::move(frame), nullptr});
        });
}

void Connection::queue_frame(Outgoing out) {
    if (closed_) {
        if (out.handler) out.handler(asio::error::not_connected);
        return;
    }
    const bool idle = write_queue_.empty();
    write_queue_.push_back(std::move(out));
    if (idle) do_write();
}

void Connection::do_write() {
    asio::async_write(socket_, asio::buffer(write_queue_.front().frame),
        [self = shared_from_this()](std::error_code ec, std::size_t) {
            Outgoing done = std::move(self->write_queue_.front());
            self->write_queue_.pop_front();

            if (ec) {
                std::deque<Outgoing> failed;
                failed.swap(self->write_queue_);
                if (done.handler) done.handler(ec);
                for (auto& out : failed)
                    if (out.handler) out.handler(ec);
                return self->teardown(ec);
            }

            if (done.handler) done.handler(ec);
            if (!self->write_queue_.empty()) self->do_write();
        });
}


void Connection::add_handler(std::string route, RequestFn fn) {
    asio::post(socket_.get_executor(),
        [self = shared_from_this(), route = std::move(route), fn = std::move(fn)]() mutable {
            if (!self->closed_) self->handlers_[std::move(route)] = std::move(fn);
        });
}

void Connection::add_event(std::string route, EventFn fn) {
    asio::post(socket_.get_executor(),
        [self = shared_from_this(), route = std::move(route), fn = std::move(fn)]() mutable {
            if (!self->closed_) self->events_[std::move(route)] = std::move(fn);
        });
}

void Connection::start() {
    do_read_length();
}

void Connection::do_read_length() {
    asio::async_read(socket_, asio::buffer(len_buf_),
        [self = shared_from_this()](std::error_code ec, std::size_t) {
            if (ec) return self->teardown(ec);

            const std::uint32_t len = decode_len(self->len_buf_.data());
            if (len > MAX_MESSAGE_SIZE)
                return self->teardown(std::make_error_code(std::errc::message_size));

            try {
                self->read_buf_.resize(len);
            } catch (const std::bad_alloc&) {
                return self->teardown(std::make_error_code(std::errc::not_enough_memory));
            }
            self->do_read_body();
        });
}

void Connection::do_read_body() {
    asio::async_read(socket_, asio::buffer(read_buf_),
        [self = shared_from_this()](std::error_code ec, std::size_t) {
            if (ec) return self->teardown(ec);

            std::istringstream body(std::move(self->read_buf_));
            Message msg;
            try {
                msg.route = read_string(body);
                const auto type = read_int<std::uint8_t>(body);
                if (type > MessageType::FAILURE)
                    throw std::runtime_error("RaptorError: unknown message type");
                msg.type = static_cast<MessageType>(type);
                msg.id = read_int<std::uint32_t>(body);
            } catch (const std::exception& e) {
                log::warn("bad frame header, closing: {}", e.what());
                return self->teardown(std::make_error_code(std::errc::bad_message));
            }

            self->dispatch(msg, body);
            if (!self->closed_) self->do_read_length();
        });
}

void Connection::dispatch(const Message& msg, std::istream& body) {
    switch (msg.type) {
        case MessageType::RESPONSE:
            finish_request(msg.id, {}, &body);
            break;

        case MessageType::FAILURE: {
            std::error_code ec = std::make_error_code(std::errc::bad_message);
            std::string reason;
            try {
                const auto code = read_int<std::uint8_t>(body);
                reason = read_string(body);
                if (code == static_cast<std::uint8_t>(RpcError::UnknownRoute) ||
                    code == static_cast<std::uint8_t>(RpcError::HandlerFailed))
                    ec = static_cast<RpcError>(code);
            } catch (const std::exception&) {
                // malformed FAILURE: still fail the call, with bad_message
            }
            if (auto it = pending_.find(msg.id); it != pending_.end())
                log::warn("rpc '{}' failed: {}", it->second.route, reason.empty() ? ec.message() : reason);
            finish_request(msg.id, ec, nullptr);
            break;
        }

        case MessageType::REQUEST: {
            std::string reply;
            if (auto it = handlers_.find(msg.route); it == handlers_.end()) {
                log::warn("rpc '{}': no handler", msg.route);
                reply = make_failure(msg.id, RpcError::UnknownRoute, "no handler for this route");
            } else {
                try {
                    reply = make_frame({}, MessageType::RESPONSE, msg.id,
                                       [&](std::ostream& out) { it->second(body, out); });
                } catch (const std::exception& e) {
                    log::warn("rpc '{}' handler failed: {}", msg.route, e.what());
                    reply = make_failure(msg.id, RpcError::HandlerFailed, e.what());
                } catch (...) {
                    log::warn("rpc '{}' handler failed: unknown exception", msg.route);
                    reply = make_failure(msg.id, RpcError::HandlerFailed, "unknown exception");
                }
            }
            if (msg.id != 0) queue_frame(Outgoing{std::move(reply), nullptr});
            break;
        }

        case MessageType::EVENT:
        case MessageType::TEXT: {
            auto it = events_.find(msg.route);
            if (it == events_.end()) {
                log::debug("event '{}': no listener, dropped", msg.route);
            } else if (!it->second(body)) {
                log::warn("event '{}': payload did not decode, dropped", msg.route);
            }
            break;
        }
    }
}

void Connection::finish_request(std::uint32_t id, std::error_code ec, std::istream* body) {
    auto it = pending_.find(id);
    if (it == pending_.end()) return;

    Completion complete = std::move(it->second.complete);
    pending_.erase(it);
    complete(ec, body);
}


void Connection::accept_handshake(Handler handler) {
    asio::async_read(socket_, asio::buffer(handshake_buf_),
        [self = shared_from_this(), handler = std::move(handler)](std::error_code ec, std::size_t) {
            if (ec) return handler(ec);

            auto& buf = self->handshake_buf_;
            if (!std::equal(RAPTOR_HEADER.begin(), RAPTOR_HEADER.end(), buf.begin()))
                return handler(std::make_error_code(std::errc::protocol_error));
            if (!std::equal(RAPTOR_VERSION.begin(), RAPTOR_VERSION.end(), buf.begin() + RAPTOR_HEADER.size()))
                return handler(std::make_error_code(std::errc::protocol_not_supported));

            handler({});
        });
}

void Connection::send_handshake(Handler handler) {
    static constexpr std::array<unsigned char, 6> hello = {
        RAPTOR_HEADER[0], RAPTOR_HEADER[1], RAPTOR_HEADER[2],
        RAPTOR_VERSION[0], RAPTOR_VERSION[1], RAPTOR_VERSION[2],
    };
    asio::async_write(socket_, asio::buffer(hello),
        [self = shared_from_this(), handler = std::move(handler)](std::error_code ec, std::size_t) {
            handler(ec);
        });
}

void Connection::close() {
    asio::post(socket_.get_executor(), [self = shared_from_this()] {
        self->teardown(asio::error::operation_aborted);
    });
}

void Connection::teardown(std::error_code ec) {
    if (closed_) return;
    closed_ = true;

    if (ec != asio::error::eof && ec != asio::error::operation_aborted)
        log::debug("connection closed: {}", ec.message());

    std::error_code ignored;
    socket_.shutdown(tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);

    handlers_.clear();
    events_.clear();

    auto pending = std::exchange(pending_, {});
    for (auto& [id, call] : pending) call.complete(ec, nullptr);
}

} // namespace raptor
