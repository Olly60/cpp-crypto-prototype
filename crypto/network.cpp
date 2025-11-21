#include <asio.hpp>
#include "crypto_utils.h"
#include <span>

// Reads exactly `numBytes` from the socket into the buffer
std::vector<uint8_t> readExact(asio::ip::tcp::socket& socket, size_t numBytes) {
    std::vector<uint8_t> buffer(numBytes);
    size_t totalRead = 0;

    while (totalRead < numBytes) {
        size_t bytesRead = socket.read_some(
            asio::buffer(buffer.data() + totalRead, numBytes - totalRead)
        );

        if (bytesRead == 0) {
            throw std::runtime_error("Connection closed before reading full data");
        }

        totalRead += bytesRead;
    }

    return buffer;
}

void handleConnection(asio::ip::tcp::socket socket) {

	// Read the protocol type (1 byte)
    std::vector<uint8_t> buffer(sizeof(uint8_t));
    buffer = readExact(socket, 1);

    uint8_t protocolType = buffer[0];

    switch (protocolType) {
    case 1: reciveBlock(socket);
    default: throw std::runtime_error("Unsupported protocol version");
    }
    
}

Block reciveBlock(asio::ip::tcp::socket& socket) {
    // Read the block size (4 bytes)
    std::vector<uint8_t> buffer(4);
	buffer = readExact(socket, 4);

    // Convert and format size from bytes to uint32_t
	uint32_t blockSize = formatNumberNative<uint32_t>(std::span<const uint8_t>(buffer.data(), buffer.size()));

    // Read the block data
	buffer.resize(blockSize);
    buffer = readExact(socket, blockSize);
    return formatBlock(buffer);
}

int main() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 8333));
    
    for (;;) {
        asio::ip::tcp::socket socket(io);
        acceptor.accept(socket);
        handleConnection(std::move(socket));
    }
}