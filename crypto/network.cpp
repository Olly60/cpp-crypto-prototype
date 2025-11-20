#include <asio.hpp>


void handleConnection(asio::ip::tcp::socket socket) {
    std::vector<uint8_t> buffer(8);
    size_t len = socket.read_some(asio::buffer(buffer));
    
}

int main() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 8333));

    for (;;) {
        asio::ip::tcp::socket socket(io);
        acceptor.accept(socket);
        handleConnection(std::move(socket));
    }
}