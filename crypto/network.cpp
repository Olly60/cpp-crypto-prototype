#include <asio.hpp>
#include "crypto_utils.h"
#include <span>

// Protocols

struct NetworkPacket {
    uint32_t protocolVersion{ 1 };
    uint32_t networkId{1};
    uint64_t services{1};
	uint64_t nonce;
    array256_t blockchainTip;

};

void handleConnection(asio::ip::tcp::socket socket) {
    
    std::vector<uint8_t> buffer;
    buffer.resize(4);
    asio::read(socket, asio::buffer(buffer.data(), 2));
    uint32_t protocolVersion = formatNumberNative<uint32_t>(std::span<const uint8_t>(buffer));
	buffer.resize(4);
	asio::read(socket, asio::buffer(buffer.data(), 4));
	uint32_t networkId = formatNumberNative<uint32_t>(std::span<const uint8_t>(buffer));
	buffer.resize(8);
	uint64_t services = formatNumberNative<uint64_t>(std::span<const uint8_t>(buffer));
	buffer.resize(8);
	asio::read(socket, asio::buffer(buffer.data(), 8));
	uint64_t nonce = formatNumberNative<uint64_t>(std::span<const uint8_t>(buffer));
	asio::read(socket, asio::buffer(buffer.data(), 32));
	array256_t blockchainTip;
	std::copy(buffer.begin(), buffer.begin() + 32, blockchainTip.begin());

    }

void sendBlock(asio::ip::tcp::socket& socket, const Block& block) {
        // Serialize the block
        std::vector<uint8_t> blockBytes = serialiseBlock(block);
        // Prepare the size prefix
        uint32_t blockSize = static_cast<uint32_t>(blockBytes.size());
        std::array<uint8_t, 4> sizeBytes = serialiseNumberLe(blockSize);
        // Send the size prefix
        asio::write(socket, asio::buffer(sizeBytes));
        // Send the block data
        asio::write(socket, asio::buffer(blockBytes));
    }

void sendTxToMempool(asio::ip::tcp::socket& socket, const Tx& tx) {

        // Serialize the transaction
        std::vector<uint8_t> txBytes = serialiseTx(tx);
        // Prepare the size prefix
        uint32_t txSize = static_cast<uint32_t>(txBytes.size());
        std::array<uint8_t, 4> sizeBytes = serialiseNumberLe(txSize);
        // Send the size prefix
        asio::write(socket, asio::buffer(sizeBytes));
        // Send the transaction data
        asio::write(socket, asio::buffer(txBytes));
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