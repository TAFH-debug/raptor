#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <asio.hpp>
#include <message.h>
#include <serializable.h>
#include <binary_io.h>

namespace raptor {

    using tcp = asio::ip::tcp;

    constexpr unsigned short PORT = 5555;

    constexpr std::uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

    constexpr std::chrono::milliseconds DEFAULT_RPC_TIMEOUT{10'000};

    enum class RpcError {
        UnknownRoute = 1,
        HandlerFailed,
    };

    const std::error_category& rpc_category();
    inline std::error_code make_error_code(RpcError e) { return {static_cast<int>(e), rpc_category()}; }

    template <class T>
    concept Payload = std::derived_from<T, Serializable> && std::default_initializable<T>;

    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        using pointer = std::shared_ptr<Connection>;
        using Handler = std::function<void(std::error_code)>;

        static pointer create(asio::io_context& io);

        tcp::socket& socket() { return socket_; }

        void send_message(std::string route, MessageType type, const Serializable& data,
                          Handler handler = nullptr);


        template <Payload Res>
        void invoke(const std::string& route, const Serializable& request,
                    std::function<void(std::error_code, Res)> on_done,
                    std::chrono::milliseconds timeout = DEFAULT_RPC_TIMEOUT) {
            const std::uint32_t id = next_request_id();

            std::string frame = encode_frame(route, MessageType::REQUEST, id, request);

            Completion complete = [on_done = std::move(on_done)](std::error_code ec, std::istream* body) {
                Res res{};
                if (!ec && !decode(*body, res)) ec = std::make_error_code(std::errc::bad_message);
                if (on_done) on_done(ec, std::move(res));
            };
            start_request(id, route, std::move(complete), timeout, std::move(frame));
        }

        template <Payload Req, class F>
            requires std::invocable<F&, const Req&> &&
                     std::derived_from<std::remove_cvref_t<std::invoke_result_t<F&, const Req&>>, Serializable>
        void register_handler(std::string route, F fn) {
            add_handler(std::move(route), [fn = std::move(fn)](std::istream& body, std::ostream& out) mutable {
                Req req{};
                req.read_binary(body);
                std::invoke(fn, std::as_const(req)).write_binary(out);
            });
        }

        template <Payload T, class F>
            requires std::invocable<F&, const T&>
        void on_event(std::string route, F fn) {
            add_event(std::move(route), [fn = std::move(fn)](std::istream& body) mutable {
                T value{};
                if (!decode(body, value)) return false;
                std::invoke(fn, std::as_const(value));
                return true;
            });
        }

        void close();

    private:
        friend class RaptorServer;
        friend class RaptorClient;

        using Completion = std::function<void(std::error_code, std::istream*)>;
        using RequestFn = std::function<void(std::istream& body, std::ostream& out)>;
        using EventFn = std::function<bool(std::istream& body)>;

        struct Pending {
            std::string route;
            Completion complete;
            std::unique_ptr<asio::steady_timer> timer;
        };

        struct Outgoing {
            std::string frame;
            Handler handler;
        };

        explicit Connection(asio::io_context& io);

        template <class T>
        static bool decode(std::istream& in, T& out) noexcept {
            try {
                out.read_binary(in);
                return true;
            } catch (...) {
                return false;
            }
        }

        static std::string encode_frame(const std::string& route, MessageType type, std::uint32_t id,
                                        const Serializable& body);
        std::uint32_t next_request_id();
        void start_request(std::uint32_t id, std::string route, Completion complete,
                           std::chrono::milliseconds timeout, std::string frame);
        void add_handler(std::string route, RequestFn fn);
        void add_event(std::string route, EventFn fn);

        void start();
        void accept_handshake(Handler handler);
        void send_handshake(Handler handler);
        void queue_frame(Outgoing out);
        void do_write();
        void do_read_length();
        void do_read_body();
        void dispatch(const Message& msg, std::istream& body);
        void finish_request(std::uint32_t id, std::error_code ec, std::istream* body);
        void teardown(std::error_code ec);

        tcp::socket socket_;
        std::deque<Outgoing> write_queue_;
        std::array<unsigned char, 6> handshake_buf_{};
        std::array<unsigned char, 4> len_buf_{};
        std::string read_buf_;
        std::atomic<std::uint32_t> next_id_{1};
        bool closed_ = false;
        std::unordered_map<std::uint32_t, Pending> pending_;
        std::unordered_map<std::string, RequestFn> handlers_;
        std::unordered_map<std::string, EventFn> events_;
    };

    class RaptorServer {
    public:
        using ConnectHandler = std::function<void(Connection::pointer)>;

        RaptorServer(asio::io_context& io, ConnectHandler on_connect, unsigned short port = PORT);
        RaptorServer(const RaptorServer&) = delete;
        RaptorServer& operator=(const RaptorServer&) = delete;

    private:
        void start_accept();
        void handle_accept(Connection::pointer connection, const std::error_code& error);

        asio::io_context& io_;
        tcp::acceptor acceptor_;
        ConnectHandler on_connect_;
    };

    class RaptorClient {
    public:
        using ConnectHandler = std::function<void(std::error_code, Connection::pointer)>;

        explicit RaptorClient(asio::io_context& io);
        RaptorClient(const RaptorClient&) = delete;
        RaptorClient& operator=(const RaptorClient&) = delete;

        void connect(const std::string& host, unsigned short port, ConnectHandler handler);
        void connect(const std::string& host, ConnectHandler handler) { connect(host, PORT, std::move(handler)); }

    private:
        asio::io_context& io_;
        tcp::resolver resolver_;
    };
}

template <>
struct std::is_error_code_enum<raptor::RpcError> : std::true_type {};
