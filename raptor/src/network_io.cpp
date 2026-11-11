#include <network_io.h>

#include <iostream>
#include <algorithm>

const char RAPTOR_HEADER[3] = {0x23, 0x41, 0x32};
const char RAPTOR_VERSION[3] = {0x00, 0x00, 0x01};

using namespace raptor;

void raptor::RaptorServer::send_message(tcp::socket socket, std::string route, MessageType type, const raptor::Serializable& data) {
    asio::streambuf buf;
    std::ostream os(&buf);

    raptor::write_string(os, route);

    raptor::write_int<int>(os, type);

    data.write_binary(os);

    size_t sz = buf.size();

    char ln_buf[4];
    for (std::size_t i = 0; i < 4; ++i)
        ln_buf[i] = static_cast<char>((sz >> (8 * i)) & 0xFF);

    asio::write(socket, asio::buffer(ln_buf));
    asio::write(socket, buf);
}

tcp::socket raptor::RaptorServer::run() {
    tcp::socket socket(io);
    acceptor.accept(socket);

    char buf[6];
    asio::read(socket, asio::buffer(buf));

    if (!std::equal(RAPTOR_HEADER, RAPTOR_HEADER + 3, buf)) {
        // LOG: invalid header
    }

    if (!std::equal(RAPTOR_VERSION, RAPTOR_VERSION + 3, buf + 3)) {
        // LOG: version dont match
    }

    return socket;
}

void raptor::RaptorClient::connect(std::string host, int port) {
    asio::connect(socket, resolver.resolve(host, std::to_string(port)));
    asio::write(socket, asio::buffer(RAPTOR_HEADER));
    asio::write(socket, asio::buffer(RAPTOR_VERSION));
}

Message raptor::RaptorClient::read_message(Serializable& data) {
    unsigned char len_buf[4];
    asio::read(socket, asio::buffer(len_buf));

    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= std::uint64_t{len_buf[i]} << (8 * i);

    asio::streambuf buf;
    std::istream is(&buf);

    asio::read(socket, buf, asio::transfer_exactly(value));

    std::string route = raptor::read_string(is);
    MessageType type = (MessageType) raptor::read_int<int>(is);
    data.read_binary(is);

    return Message { route, type };
}