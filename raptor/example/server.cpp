#include <serializable.h>
#include <macro_serialize.h>
#include <network_io.h>
#include <asio.hpp>
#include <log.h>

#include <iostream>

class Vector : public raptor::Serializable {
public:
    int x;
    int y;
    float z;

    RAPTOR_CLASS(x, y, z)
};

class A : public raptor::Serializable {
public:
    int a;
    int b;
    Vector vec;

    RAPTOR_CLASS(a, b, vec)
};

using namespace raptor;

A some_rpc_func(A a) {
    return a;
}

int main() {
    asio::io_context io;

    A a;
    raptor::RaptorServer server(io, [&a](Connection::pointer conn) {
        // conn->send_message("aboba", MessageType::EVENT, a, [](std::error_code ec) {
        //     if (ec) std::cerr << "send failed: " << ec.message() << "\n";
        // });

        conn->read_message(a, [&a, conn](std::error_code ec, Message msg) {
            if (ec) {
                std::cerr << "read failed: " << ec.message() << "\n";
                return;
            }
            if (msg.type == MessageType::REQUEST) {
                conn->send_message(msg.route, MessageType::RESPONSE, some_rpc_func(a));
            }
        });
    });

    io.run();
}