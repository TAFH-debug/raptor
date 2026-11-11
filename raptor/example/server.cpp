#include <serializable.h>
#include <macro_serialize.h>
#include <network_io.h>
#include <asio.hpp>

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
    raptor::RaptorServer server;

    A a;
    a.vec.x = 200;
    a.vec.y = 300;
    a.vec.z = 20.51;
    server.send_message(server.run(), "aboba", MessageType::EVENT, a);
}