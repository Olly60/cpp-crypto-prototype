#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include <random>
#include "network/network.h"
#include "storage/peers.h"
#include "block_verification.h"
#include "storage/block/block_utils.h"
#include "storage/block/genesis_block.h"
#include "storage/block/tip_block.h"

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
    BroadcastMempoolTx = 6,
    GetMempool = 7,
    GetHeaders = 8
};

struct Array256Hash {
    size_t operator()(const Array256_t& a) const {
        // Simple xor-folding over 8-byte chunks
        size_t result = 0;
        for (size_t i = 0; i < 32; i += 8) {
            size_t chunk = 0;
            for (size_t j = 0; j < 8; ++j) {
                chunk <<= 8;
                chunk |= a[i + j];
            }
            result ^= chunk;
        }
        return result;
    }
};

// ============================================
// Global State
// ============================================

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
std::unordered_map<Array256_t, Tx, Array256Hash> mempool;

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
        getTipHash()
    };
}

std::vector<uint8_t> serialiseHandshake(const Handshake& hs)
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

// ============================================
// Add peer to peer map in memory
// ============================================

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
// Request
// ============================================

asio::awaitable<void> requestHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::Handshake);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Send our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read peer handshake
        std::vector<uint8_t> buffer(sizeof(Handshake));
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);

        const Handshake theirHandshake = parseHandshake(buffer);
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
    catch (const std::exception&)
    {
    }
}

asio::awaitable<void> requestPing(asio::ip::tcp::socket& socket)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::Ping);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
    } catch (const std::exception&)
    {
    }
}

asio::awaitable<std::optional<BlockHeader>> requestBlockHeader(
    asio::ip::tcp::socket& socket,
    const Array256_t& blockHash
)
{
    try
    {
        // Send request
        auto msgType = ProtocolMessage::GetHeader;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have header
        uint8_t hasHeader;
        co_await asio::async_read(socket, asio::buffer(&hasHeader, 1), asio::use_awaitable);
        if (hasHeader == 0) { co_return std::nullopt; }

        // Read size
        uint64_t headerSize;
        std::array<uint8_t, 8> sizeBuf{};
        co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
        takeBytesInto(headerSize, sizeBuf);

        // Read header
        std::vector<uint8_t> headerBytes(headerSize);
        co_await asio::async_read(socket, asio::buffer(headerBytes), asio::use_awaitable);

        co_return parseBlockHeader(headerBytes);
    }
    catch (const std::exception&)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}


asio::awaitable<std::optional<Block>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash)
{
    try
    {
        // Send request
        auto msgType = static_cast<uint8_t>(ProtocolMessage::GetBlock);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Block hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have block
        uint8_t hasBlock;
        co_await asio::async_read(socket, asio::buffer(&hasBlock, 1), asio::use_awaitable);
        if (hasBlock == 0) { co_return std::nullopt; }

        // Read size
        uint64_t blockSize;
        std::array<uint8_t, 8> sizeBuf{};
        co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
        takeBytesInto(blockSize, sizeBuf);

        // Read block
        std::vector<uint8_t> blockBytes(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockBytes), asio::use_awaitable);

        co_return parseBlock(blockBytes);
    }
    catch (const std::exception&)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}

asio::awaitable<std::optional<Block>> requestHeaders(asio::ip::tcp::socket& socket, const std::vector<BlockHeader>& commonAncestor)
{ try {
 //TODO: make function
} catch (const std::exception&)
{
}
}

asio::awaitable<std::vector<Tx>> requestMempool(asio::ip::tcp::socket& socket)
{
    try {
    // Read inv size
    uint64_t invSize;
    std::array<uint8_t, 8> sizeBuf{};
    co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
    takeBytesInto(invSize, sizeBuf);

    // Read inv
    std::vector<uint8_t> theirInv(sizeof(Array256_t) * invSize);
    co_await asio::async_read(socket, asio::buffer(theirInv), asio::use_awaitable);

    for (uint64_t i = 0; i == invSize; i++)
    if (mempool.find()
    {
        mempool.push_back(newTx);
        // Note: broadcastTransaction would need to be implemented
    }

    // Ask for missing transactions
    co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

    else
    {
        throw std::runtime_error("Invalid transaction");
    }
} catch (const std::exception&)
{
}

}

// ============================================
// Handle requests
// ============================================

asio::awaitable<void> handleGetHeader(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check header is in storage
        if (!blockExists(blockHash))
        {
            uint8_t haveHeader = 0;
            co_await asio::async_write(socket, asio::buffer(&haveHeader, 1), asio::use_awaitable);
            co_return;
        }

        // Tell peer I have it
        uint8_t haveBlock = 1;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

        // Get header from storage
        auto headerBytes = readBlockFileHeaderBytes(blockHash);

        // Send size
        const uint64_t headerSize = headerBytes.size();
        std::vector<uint8_t> sizeBuf;
        appendBytes(sizeBuf, headerSize);
        co_await asio::async_write(socket, asio::buffer(sizeBuf), asio::use_awaitable);

        // Send header
        co_await asio::async_write(socket, asio::buffer(headerBytes), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to send header
    }
}

asio::awaitable<void> handleGetBlock(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check block is in storage
        if (!blockExists(blockHash))
        {
            uint8_t haveBlock = 0;
            co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);
            co_return;
        }
        // Tell peer I have it
        uint8_t haveBlock = 1;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

        // Get block from storage
        auto blockData = readBlockFileBytes(blockHash);

        // Send size
        const uint64_t blockSize = blockData.size();
        std::vector<uint8_t> sizeBuf;
        appendBytes(sizeBuf, blockSize);
        co_await asio::async_write(socket, asio::buffer(sizeBuf), asio::use_awaitable);

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockData), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to send block
    }
}

asio::awaitable<void> handleBroadcastBlock(asio::ip::tcp::socket& socket)
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

        if (const Block newBlock = parseBlock(blockData); verifyBlock(newBlock))
        {
            addBlock(newBlock);
            // Note: broadcastBlockToPeers would need to be implemented
        }
        else if (!blockExists(getBlockHash(newBlock)))
        {
            throw std::runtime_error("Invalid block");
        }
    }
    catch (const std::exception&)
    {
        // Failed to process block
    }
}

asio::awaitable<void> handleBroadcastMempoolTx(asio::ip::tcp::socket& socket)
{
    try
    {
        //TODO: make function
    }
    catch (const std::exception&)
    {
        // Failed to process transaction
    }
}

asio::awaitable<void> handleHandshake(asio::ip::tcp::socket& socket)
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
        auto myHandshake = serialiseHandshake(createHandshake());
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
    catch (const std::exception&)
    {
        // Connection failed
    }
}

asio::awaitable<void> handlePing(asio::ip::tcp::socket& socket)
{
    try
    {
        uint8_t pong = 0x01;
        co_await asio::async_write(socket, asio::buffer(&pong, 1), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to respond
    }
}

asio::awaitable<void> handleGetHeaders(asio::ip::tcp::socket& socket)
{
    try {
        //TODO: make function
    } catch (const std::exception&)
    {
    }
}

asio::awaitable<void> handleGetMempool(asio::ip::tcp::socket& socket)
{
    try {
        //TODO: make function
    } catch (const std::exception&)
    {
    }
}

// ============================================
// Broadcast new data
// ============================================

asio::awaitable<void> BroadcastMempoolTx()
{
    try {
        //TODO: make function
    } catch (const std::exception&)
    {
    }
}

asio::awaitable<void> BroadcastNewBlock()
{
    try {
        //TODO: make function
    } catch (const std::exception&)
    {
    }
}

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<void> syncIfBetter(asio::ip::tcp::socket& socket)
{
    try {
        //TODO: make function

        // if work they claim > then verify else ignore
        //
        //    // TODO: Compare block chain work
        //    requestBlock()
        //    co_await asio::async_read(socket, asio::use_awaitable);
        //
        //    // if their work > our work
        //    // TODO: Remove blocks up to matching one
        //
        //    // TODO: Add new block
    }
    catch (const std::exception&)
    {
    }

}

// ============================================
// Handle connection
// ============================================

asio::awaitable<void> handleConnection(asio::ip::tcp::socket& socket)
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
            co_await handleHandshake(socket);
            co_return;
        }

        // Check if peer is authenticated
        if (!peers.contains(peerAddr))
        {
            co_return; // Unauthenticated peer
        }

        // Update last seen
        peers[peerAddr].lastSeen = getCurrentTimestamp();

        // Route message
        switch (static_cast<ProtocolMessage>(msgType))
        {
        case ProtocolMessage::Ping:
            co_await handlePing(socket);
            break;
        case ProtocolMessage::GetHeader:
            co_await handleGetHeader(socket);
            break;
        case ProtocolMessage::GetBlock:
            co_await handleGetBlock(socket);
            break;
        case ProtocolMessage::BroadcastBlock:
            co_await handleBroadcastBlock(socket);
            break;
        case ProtocolMessage::BroadcastMempoolTx:
            co_await handleBroadcastMempoolTx(socket);
            break;
        case ProtocolMessage::GetMempool:
            co_await handleGetMempool(socket);
            break;
        case ProtocolMessage::GetHeaders:
            co_await handleGetHeaders(socket);
            break;
        default:
            break; // Unknown message
        }
    }
    catch (const std::exception&)
    {
        // Connection error
    }
}

// ============================================
// Accept connections
// ============================================

asio::awaitable<void> acceptConnections(asio::ip::tcp::acceptor& acceptor)
{
    try
    {
        for (;;)
        {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            co_spawn(acceptor.get_executor(),
                     handleConnection(socket),
                     asio::detached);
        }
    }
    catch (const asio::system_error& e)
    {
        if (e.code() != asio::error::operation_aborted)
            throw;
    }
}


// ============================================
// Main
// ============================================

int main()
{
    asio::io_context ioContext;
    asio::ip::tcp::acceptor acceptor(ioContext,
                                     asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

    co_spawn(ioContext, acceptConnections(acceptor), asio::detached);

    ioContext.run();

    storePeers(peers);
}
