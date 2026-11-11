#pragma once

#include <asio.hpp>
#include <message.h>
#include <serializable.h>
#include <binary_io.h>

using asio::ip::tcp;

namespace raptor {

    const int PORT = 5555;

    class RaptorServer {
    public:
        RaptorServer() : acceptor(io, {tcp::v4(), PORT}) {}

        tcp::socket run();
        void send_message(tcp::socket socket, std::string route, MessageType type, const raptor::Serializable& data);
    private:
        asio::io_context io;
        tcp::acceptor acceptor;
    };

    class RaptorClient {
    public:
        RaptorClient() : socket(io), resolver(io) {}

        void connect(std::string host, int port=PORT);

        void _print_recv();

        Message read_message(Serializable& data);
    private:
        asio::io_context io;
        asio::ip::tcp::socket socket;
        asio::ip::tcp::resolver resolver;
    };
}