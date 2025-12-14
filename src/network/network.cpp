#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network.h"
#include "storage/block/tip_block.h"
#include "network/request.h"
#include "network/network.h"

#include <random>

#include "storage/peers.h"
#include "storage/block/genesis_block.h"

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

struct Array256Hash
{
    size_t operator()(const Array256_t& a) const
    {
        // Simple xor-folding over 8-byte chunks
        size_t result = 0;
        for (size_t i = 0; i < 32; i += 8)
        {
            size_t chunk = 0;
            for (size_t j = 0; j < 8; ++j)
            {
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
// Broadcast new data
// ============================================

asio::awaitable<void> BroadcastNewTx()
{
    try
    {
        //TODO: make function
    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<void> BroadcastNewBlock()
{
    try
    {
        //TODO: make function
    }
    catch (const std::exception&)
    {
    }
}

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<void> syncIfBetter(asio::ip::tcp::socket& socket)
{
    try
    {
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
            co_await handleNewBlock(socket);
            break;
        case ProtocolMessage::BroadcastMempoolTx:
            co_await handleNewTx(socket);
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
