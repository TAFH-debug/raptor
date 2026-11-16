#include <serializable.h>
#include <macro_serialize.h>
#include <network_io.h>
#include <asio.hpp>

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

int main() {
    asio::io_context io;

    raptor::RaptorServer server(io, [](Connection::pointer conn) {
        A a;
        a.vec.x = 200;
        a.vec.y = 300;
        a.vec.z = 20.51;

        conn->send_message("aboba", MessageType::EVENT, a, [](std::error_code ec) {
            if (ec) std::cerr << "send failed: " << ec.message() << "\n";
        });
    });

    io.run();
}