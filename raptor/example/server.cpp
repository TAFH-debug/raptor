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

using namespace raptor;

A some_rpc_func(A a) {
    a.vec.z += 10;
    return a;
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
