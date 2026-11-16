#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include <asio.hpp>
#include <message.h>
#include <serializable.h>
#include <binary_io.h>

namespace raptor {

    using tcp = asio::ip::tcp;

    constexpr unsigned short PORT = 5555;

    // Upper bound for an incoming frame, protects against bogus length prefixes.
    constexpr std::uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        using pointer = std::shared_ptr<Connection>;
        using Handler = std::function<void(std::error_code)>;
        using ReadHandler = std::function<void(std::error_code, Message)>;

        static pointer create(asio::io_context& io);

        tcp::socket& socket() { return socket_; }

        // Serializes `data` immediately, then queues the frame for sending.
        // Frames are sent in call order. `data` may be destroyed right after the call.
        void send_message(std::string route, MessageType type, const Serializable& data,
                          Handler handler = nullptr);

        // Reads one frame and deserializes the payload into `data`.
        // `data` must stay alive until `handler` runs. One read at a time per connection.
        void read_message(Serializable& data, ReadHandler handler);

        void close();

    private:
        friend class RaptorServer;
        friend class RaptorClient;

        explicit Connection(asio::io_context& io);

        void accept_handshake(Handler handler);
        void send_handshake(Handler handler);
        void do_write();
        void do_read_length(Serializable& data, ReadHandler handler);
        void do_read_body(Serializable& data, ReadHandler handler);

        struct Outgoing {
            std::string frame;
            Handler handler;
        };

        tcp::socket socket_;
        std::deque<Outgoing> write_queue_;
        std::array<unsigned char, 6> handshake_buf_{};
        std::array<unsigned char, 4> len_buf_{};
        std::string read_buf_;
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
