#include <macro_serialize.h>
#include <network_io.h>
#include <log.h>

#include <memory>

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

class B : public Serializable {
public:
    int c;
    int g;

    RAPTOR_CLASS(c, g)
};

int main() {
    asio::io_context io;
    raptor::RaptorClient client(io);

    A a{};
    a.vec.x = 200;
    a.vec.y = 300;
    a.vec.z = 20.51f;

    client.connect("localhost", [a](std::error_code ec, Connection::pointer conn) {
        if (ec) {
            raptor::log::error("connect failed: {}", ec.message());
            return;
        }

        conn->on_event<A>("welcome", [](const A&) { raptor::log::info("server says welcome"); });

        constexpr int calls = 3;
        auto remaining = std::make_shared<int>(calls + 1);
        auto done = [conn, remaining] { if (--*remaining == 0) conn->close(); };

        for (int i = 0; i < calls; ++i) {
            A req = a;
            req.a = i;
            conn->invoke<B>("some_rpc_func", req, [i, done](std::error_code ec, B res) {
                if (ec) raptor::log::error("call {} failed: {}", i, ec.message());
                else raptor::log::info("call {}: a={} z={}", i, res.c, res.g);
                done();
            });
        }

        conn->invoke<A>("no_such_func", a, [done](std::error_code ec, A) {
            raptor::log::info("no_such_func -> {}", ec.message());
            done();
        });
    });

    io.run();
}
