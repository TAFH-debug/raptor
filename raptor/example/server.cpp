#include <macro_serialize.h>
#include <network_io.h>
#include <log.h>

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

class B : public raptor::Serializable {
public:
    int c;
    int g;

    RAPTOR_CLASS(c, g)
};

using namespace raptor;

B some_rpc_func(A a) {
    B b{};
    b.c = 20;
    b.g = a.vec.z / 0;
    return b;
}

int main() {
    asio::io_context io;

    raptor::RaptorServer server(io, [](Connection::pointer conn) {
        conn->register_handler<A>("some_rpc_func", some_rpc_func);

        conn->send_message("welcome", MessageType::EVENT, A{});
    });

    raptor::log::info("listening on port {}", PORT);
    io.run();
}
