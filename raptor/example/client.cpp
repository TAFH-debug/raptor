#include <serializable.h>
#include <macro_serialize.h>
#include <network_io.h>

#include <fstream>
#include <iostream>

using namespace raptor;

class Vector : public Serializable {
public:
    int x;
    int y;
    float z;

    RAPTOR_CLASS(x, y, z)
};

class A : public Serializable {
public:
    int a;
    int b;
    Vector vec;

    RAPTOR_CLASS(a, b, vec)
};

int main() {
    asio::io_context io;
    raptor::RaptorClient client(io);

    A a;
    a.vec.x = 200;
    a.vec.y = 300;
    a.vec.z = 20.51;
    client.connect("localhost", [&a](std::error_code ec, Connection::pointer conn) {
        if (ec) {
            std::cerr << "connect failed: " << ec.message() << "\n";
            return;
        }

        // conn->read_message(a, [&a, conn](std::error_code ec, Message msg) {
        //     if (ec) {
        //         std::cerr << "read failed: " << ec.message() << "\n";
        //         return;
        //     }
        //     std::cout << msg.route << " " << msg.type << " " << a.vec.z << "\n";
        // });

        conn->invoke<A>("some_rpc_func", a, [](A b) {
            std::cout << b.vec.z << "\n";
        });
    });

    io.run();
}