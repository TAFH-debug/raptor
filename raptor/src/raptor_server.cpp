#include <network_io.h>
#include <log.h>

namespace raptor {

RaptorServer::RaptorServer(asio::io_context& io, ConnectHandler on_connect, unsigned short port)
    : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), on_connect_(std::move(on_connect)) {
    start_accept();
}

void RaptorServer::start_accept() {
    Connection::pointer connection = Connection::create(io_);

    acceptor_.async_accept(connection->socket(),
        [this, connection](const std::error_code& error) { handle_accept(connection, error); });
}

void RaptorServer::handle_accept(Connection::pointer connection, const std::error_code& error) {
    if (error == asio::error::operation_aborted) return;

    if (error) {
        log::warn("accept failed: {}", error.message());
    } else {
        connection->accept_handshake([this, connection](std::error_code ec) {
            if (ec) {
                log::warn("handshake rejected: {}", ec.message());
                return connection->close();
            }
            on_connect_(connection);
            connection->start();
        });
    }

    start_accept();
}

} // namespace raptor
