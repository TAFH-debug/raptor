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
    raptor::RaptorClient client;

    A a;
    client.connect("localhost");
    Message msg = client.read_message(a);
    std::cout << msg.route << " " << msg.type << " " << a.vec.z << "\n";
}