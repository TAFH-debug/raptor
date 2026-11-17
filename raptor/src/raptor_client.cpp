#include <network_io.h>

namespace raptor {

RaptorClient::RaptorClient(asio::io_context& io) : io_(io), resolver_(io) {}

void RaptorClient::connect(const std::string& host, unsigned short port, ConnectHandler handler) {
    resolver_.async_resolve(host, std::to_string(port),
        [this, handler = std::move(handler)](std::error_code ec, tcp::resolver::results_type endpoints) mutable {
            if (ec) return handler(ec, nullptr);

            auto connection = Connection::create(io_);
            asio::async_connect(connection->socket(), endpoints,
                [connection, handler = std::move(handler)](std::error_code ec, const tcp::endpoint&) mutable {
                    if (ec) return handler(ec, nullptr);

                    connection->send_handshake(
                        [connection, handler = std::move(handler)](std::error_code ec) {
                            if (ec) return handler(ec, nullptr);
                            handler({}, connection);
                            connection->start();
                        });
                });
        });
}

}
