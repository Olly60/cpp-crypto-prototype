#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include <random>
#include "network/network.h"
#include "storage/peers.h"
#include "block_verification.h"
#include "block_verification/block_verification.h"
#include "storage/block/block_utils.h"

// ============================================
// Data Structures
// ============================================

struct Handshake
{
    uint64_t Version;
    Array256_t genesisBlockHash;
    uint64_t services;
    uint64_t nonce;
    Array256_t blockchainTip;
};

enum class ProtocolMessage : uint8_t
{
    Handshake = 1,
    Ping = 2,
    GetHeader = 3,
    GetBlock = 4,
    BroadcastBlock = 5,
    BroadcastTransaction = 6,
    GetMempool = 7,
};

// ============================================
// Global State
// ============================================

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
std::vector<Tx> mempool;

constexpr uint64_t SERVICE_FULL_NODE = 0b1;
constexpr uint64_t PROTOCOL_VERSION = 1;
const Array256_t GENESIS_BLOCK_HASH = getGenesisBlockHash();

uint64_t generateLocalNonce()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    return dist(gen);
}

const uint64_t LOCAL_NONCE = generateLocalNonce();

// ============================================
// Serialization Helpers
// ============================================

Handshake createHandshake()
{
    return {
        PROTOCOL_VERSION,
        GENESIS_BLOCK_HASH,
        SERVICE_FULL_NODE,
        LOCAL_NONCE,
        getBlockchainTip()
    };
}

std::vector<uint8_t> serializeHandshake(const Handshake& hs)
{
    std::vector<uint8_t> buffer;
    appendBytes(buffer, hs.Version);
    appendBytes(buffer, hs.genesisBlockHash);
    appendBytes(buffer, hs.services);
    appendBytes(buffer, hs.nonce);
    appendBytes(buffer, hs.blockchainTip);
    return buffer;
}

Handshake parseHandshake(const std::vector<uint8_t>& buffer)
{
    Handshake hs{};
    size_t offset = 0;
    takeBytesInto(hs.Version, buffer, offset);
    takeBytesInto(hs.genesisBlockHash, buffer, offset);
    takeBytesInto(hs.services, buffer, offset);
    takeBytesInto(hs.nonce, buffer, offset);
    takeBytesInto(hs.blockchainTip, buffer, offset);
    return hs;
}

bool isValidHandshake(const Handshake& hs)
{
    return
        hs.Version == PROTOCOL_VERSION &&
        hs.genesisBlockHash == GENESIS_BLOCK_HASH &&
        hs.nonce != LOCAL_NONCE;
}

void addPeer(const asio::ip::tcp::socket& socket, const Handshake& hs)
{
    const PeerAddress addr{
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()
    };

    const PeerStatus status{
        hs.services,
        getCurrentTimestamp()
    };

    peers[addr] = status;
}

// ============================================
// Coroutine-based Protocol (C++20)
// ============================================

asio::awaitable<void> handleHandshakeResponder(asio::ip::tcp::socket socket)
{
    try
    {
        // Read peer handshake
        std::vector<uint8_t> buffer(sizeof(Handshake));
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);

        const Handshake theirHandshake = parseHandshake(buffer);
        if (!isValidHandshake(theirHandshake))
        {
            co_return;
        }

        // Send our handshake
        auto myHandshake = serializeHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read verack
        uint8_t theirVerack;
        co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
        if (theirVerack != 0x01)
        {
            co_return;
        }

        // Send verack
        uint8_t myVerack = 0x01;
        co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

        addPeer(socket, theirHandshake);
    }
    catch (const std::exception& e)
    {
        // Connection failed
    }
}

asio::awaitable<void> handleHandshakeInitiator(asio::ip::tcp::socket socket)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::Handshake);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Send our handshake
        auto myHandshake = serializeHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read peer handshake
        std::vector<uint8_t> buffer(sizeof(Handshake));
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);

        Handshake theirHandshake = parseHandshake(buffer);
        if (!isValidHandshake(theirHandshake))
        {
            co_return;
        }

        // Send verack
        uint8_t myVerack = 0x01;
        co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

        // Read verack
        uint8_t theirVerack;
        co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
        if (theirVerack != 0x01)
        {
            co_return;
        }

        addPeer(socket, theirHandshake);
    }
    catch (const std::exception& e)
    {
        // Connection failed
    }
}

asio::awaitable<void> handlePing(asio::ip::tcp::socket socket)
{
    try
    {
        uint8_t pong = 0x01;
        co_await asio::async_write(socket, asio::buffer(&pong, 1), asio::use_awaitable);
    }
    catch (const std::exception& e)
    {
        // Failed to respond
    }
}

asio::awaitable<BlockHeader> requestBlockHeader(asio::ip::tcp::socket& socket, const Array256_t& blockHash)
{
    // Send request
    auto msgType = static_cast<uint8_t>(ProtocolMessage::GetHeader);
    co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
    co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

    // Read size
    uint64_t headerSize;
    std::array<uint8_t, 8> sizeBuf{};
    co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
    takeBytesInto(headerSize, sizeBuf);

    // Read header
    std::vector<uint8_t> headerBytes(headerSize);
    co_await asio::async_read(socket, asio::buffer(headerBytes), asio::use_awaitable);

    co_return formatBlockHeader(headerBytes);
}

asio::awaitable<Block> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash)
{
    // Send request
    auto msgType = static_cast<uint8_t>(ProtocolMessage::GetBlock);
    co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
    co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

    // Read size
    uint64_t blockSize;
    std::array<uint8_t, 8> sizeBuf{};
    co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
    takeBytesInto(blockSize, sizeBuf);

    // Read block
    std::vector<uint8_t> blockBytes(blockSize);
    co_await asio::async_read(socket, asio::buffer(blockBytes), asio::use_awaitable);

    co_return formatBlock(blockBytes);
}

asio::awaitable<void> handleGetHeader(asio::ip::tcp::socket socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Get header from storage
        auto headerData = readBlockFileHeader(blockHash);
        const auto blockHeader = formatBlockHeader(headerData);
        auto headerBytes = serialiseBlockHeader(blockHeader);

        // Send size
        uint64_t headerSize = headerBytes.size();
        std::vector<uint8_t> sizeBuf;
        appendBytes(sizeBuf, headerSize);
        co_await asio::async_write(socket, asio::buffer(sizeBuf), asio::use_awaitable);

        // Send header
        co_await asio::async_write(socket, asio::buffer(headerBytes), asio::use_awaitable);
    }
    catch (const std::exception& e)
    {
        // Failed to send header
    }
}

asio::awaitable<void> handleGetBlock(asio::ip::tcp::socket socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Get block from storage
        auto blockData = readBlockFile(blockHash);

        // Send size
        uint64_t blockSize = blockData.size();
        std::vector<uint8_t> sizeBuf;
        appendBytes(sizeBuf, blockSize);
        co_await asio::async_write(socket, asio::buffer(sizeBuf), asio::use_awaitable);

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockData), asio::use_awaitable);
    }
    catch (const std::exception& e)
    {
        // Failed to send block
    }
}

asio::awaitable<void> handleBroadcastBlock(asio::ip::tcp::socket socket)
{
    try
    {
        // Read size
        uint64_t blockSize;
        std::array<uint8_t, 8> sizeBuf{};
        co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
        takeBytesInto(blockSize, sizeBuf);

        // Read block
        std::vector<uint8_t> blockData(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockData), asio::use_awaitable);

        if (const Block newBlock = formatBlock(blockData); verifyBlock(newBlock))
        {
            addBlock(newBlock);
            // Note: broadcastBlockToPeers would need to be implemented
        }
        else if (!blockExists(getBlockHash(newBlock)))
        {
            throw std::runtime_error("Invalid block");
        }
    }
    catch (const std::exception& e)
    {
        // Failed to process block
    }
}

asio::awaitable<void> handleBroadcastTransaction(asio::ip::tcp::socket socket)
{
    try
    {
        // Read size
        uint64_t txSize;
        std::array<uint8_t, 8> sizeBuf{};
        co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
        takeBytesInto(txSize, sizeBuf);

        // Read transaction
        std::vector<uint8_t> txData(txSize);
        co_await asio::async_read(socket, asio::buffer(txData), asio::use_awaitable);

        Tx newTx = formatTx(txData);
        if (verifyTx(newTx))
        {
            mempool.push_back(newTx);
            // Note: broadcastTransaction would need to be implemented
        }
        else
        {
            throw std::runtime_error("Invalid transaction");
        }
    }
    catch (const std::exception& e)
    {
        // Failed to process transaction
    }
}

asio::awaitable<void> handleConnection(asio::ip::tcp::socket socket)
{
    try
    {
        const PeerAddress peerAddr{
            socket.remote_endpoint().address().to_string(),
            socket.remote_endpoint().port()
        };

        // Read message type
        uint8_t msgType;
        co_await asio::async_read(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Handle handshake
        if (msgType == 1)
        {
            co_await handleHandshakeResponder(std::move(socket));
            co_return;
        }

        // Check if peer is authenticated
        if (peers.find(peerAddr) == peers.end())
        {
            co_return; // Unauthenticated peer
        }

        // Update last seen
        peers[peerAddr].lastSeen = getCurrentTimestamp();

        // Route message
        switch (static_cast<ProtocolMessage>(msgType))
        {
        case ProtocolMessage::Ping:
            co_await handlePing(std::move(socket));
            break;
        case ProtocolMessage::GetHeader:
            co_await handleGetHeader(std::move(socket));
            break;
        case ProtocolMessage::GetBlock:
            co_await handleGetBlock(std::move(socket));
            break;
        case ProtocolMessage::BroadcastBlock:
            co_await handleBroadcastBlock(std::move(socket));
            break;
        case ProtocolMessage::BroadcastTransaction:
            co_await handleBroadcastTransaction(std::move(socket));
            break;
        default:
            break; // Unknown message
        }
    }
    catch (const std::exception& e)
    {
        // Connection error
    }
}

asio::awaitable<void> acceptConnections(asio::ip::tcp::acceptor& acceptor)
{
    while (true)
    {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        co_spawn(acceptor.get_executor(),
                 handleConnection(std::move(socket)),
                 asio::detached);
    }
}

// ============================================
// Main
// ============================================

int main()
{
    try
    {
        asio::io_context ioContext;
        asio::ip::tcp::acceptor acceptor(ioContext,
                                         asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

        co_spawn(ioContext, acceptConnections(acceptor), asio::detached);

        ioContext.run();
    }
    catch (const std::exception& e)
    {
    }
}
