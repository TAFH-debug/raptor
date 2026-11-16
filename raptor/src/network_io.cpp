#include <network_io.h>

#include <algorithm>
#include <sstream>

namespace raptor {

namespace {

constexpr std::array<unsigned char, 3> RAPTOR_HEADER = {0x23, 0x41, 0x32};
constexpr std::array<unsigned char, 3> RAPTOR_VERSION = {0x00, 0x00, 0x01};

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

} // namespace

Connection::Connection(asio::io_context& io) : socket_(asio::make_strand(io)) {}

Connection::pointer Connection::create(asio::io_context& io) {
    return pointer(new Connection(io));
}

void Connection::send_message(std::string route, MessageType type, const Serializable& data, Handler handler) {
    std::ostringstream os;
    os.write("\0\0\0\0", kLenWidth); // length placeholder, patched below
    write_string(os, route);
    write_int<int>(os, type);
    data.write_binary(os);

    std::string frame = std::move(os).str();
    std::size_t payload_size = frame.size() - kLenWidth;
    if (payload_size > MAX_MESSAGE_SIZE)
        throw std::runtime_error("RaptorError: message too large");
    encode_len(static_cast<std::uint32_t>(payload_size), frame.data());

    asio::post(socket_.get_executor(),
        [self = shared_from_this(), out = Outgoing{std::move(frame), std::move(handler)}]() mutable {
            bool idle = self->write_queue_.empty();
            self->write_queue_.push_back(std::move(out));
            if (idle) self->do_write();
        });
}

void Connection::do_write() {
    asio::async_write(socket_, asio::buffer(write_queue_.front().frame),
        [self = shared_from_this()](std::error_code ec, std::size_t) {
            Outgoing done = std::move(self->write_queue_.front());
            self->write_queue_.pop_front();

            if (ec) {
                // Socket is broken: fail every queued frame.
                std::deque<Outgoing> failed;
                failed.swap(self->write_queue_);
                if (done.handler) done.handler(ec);
                for (auto& out : failed)
                    if (out.handler) out.handler(ec);
                return;
            }

            if (done.handler) done.handler(ec);
            if (!self->write_queue_.empty()) self->do_write();
        });
}

void Connection::read_message(Serializable& data, ReadHandler handler) {
    asio::post(socket_.get_executor(),
        [self = shared_from_this(), &data, handler = std::move(handler)]() mutable {
            self->do_read_length(data, std::move(handler));
        });
}

void Connection::do_read_length(Serializable& data, ReadHandler handler) {
    asio::async_read(socket_, asio::buffer(len_buf_),
        [self = shared_from_this(), &data, handler = std::move(handler)](std::error_code ec, std::size_t) mutable {
            if (ec) return handler(ec, Message{});

            std::uint32_t len = decode_len(self->len_buf_.data());
            if (len > MAX_MESSAGE_SIZE)
                return handler(std::make_error_code(std::errc::message_size), Message{});

            self->read_buf_.resize(len);
            self->do_read_body(data, std::move(handler));
        });
}

void Connection::do_read_body(Serializable& data, ReadHandler handler) {
    asio::async_read(socket_, asio::buffer(read_buf_),
        [self = shared_from_this(), &data, handler = std::move(handler)](std::error_code ec, std::size_t) mutable {
            if (ec) return handler(ec, Message{});

            Message msg;
            try {
                std::istringstream is(std::move(self->read_buf_));
                msg.route = read_string(is);
                msg.type = static_cast<MessageType>(read_int<int>(is));
                data.read_binary(is);
            } catch (const std::exception&) {
                return handler(std::make_error_code(std::errc::bad_message), Message{});
            }
            handler({}, std::move(msg));
        });
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
        std::error_code ignored;
        self->socket_.shutdown(tcp::socket::shutdown_both, ignored);
        self->socket_.close(ignored);
    });
}

RaptorClient::RaptorClient(asio::io_context& io) : io_(io), resolver_(io) {}

void RaptorClient::connect(const std::string& host, unsigned short port, ConnectHandler handler) {
    resolver_.async_resolve(host, std::to_string(port),
        [this, handler = std::move(handler)](std::error_code ec, tcp::resolver::results_type endpoints) mutable {
            if (ec) return handler(ec, nullptr);

            auto connection = Connection::create(io_);
            asio::async_connect(connection->socket(), endpoints,
                [connection, handler = std::move(handler)](std::error_code ec, const tcp::endpoint&) mutable {
                    if (ec) return handler(ec, nullptr);

                    connection->send_handshake(
                        [connection, handler = std::move(handler)](std::error_code ec) {
                            if (ec) return handler(ec, nullptr);
                            handler({}, connection);
                        });
                });
        });
}

} // namespace raptor
