#pragma once
#include "storage/peers.h"
#include <random>
#include <unordered_set>
#include <asio/awaitable.hpp>
#include "storage/block/genesis_block.h"

// ============================================
// Protocol messages
// ============================================
namespace ProtocolMessage
{
    constexpr uint8_t CommandSize = 16;

    constexpr std::array<uint8_t, CommandSize> makeCommand(const char* str) {
    std::array<uint8_t, CommandSize> a{};
    for (size_t i = 0; i < CommandSize && str[i] != '\0'; ++i) {
        a[i] = static_cast<uint8_t>(str[i]);
    }
    return a;
}
    constexpr auto Handshake = makeCommand("handshake");
    constexpr auto Ping = makeCommand("ping");
    constexpr auto GetHeader= makeCommand("getheader");
    constexpr auto GetBlock = makeCommand("getblock");
    constexpr auto BroadcastNewBlock = makeCommand("broadcastnewblock");
    constexpr auto BroadcastNewTx = makeCommand("broadcastnewtx");
    constexpr auto GetMempool = makeCommand("getmempool");
    constexpr auto GetHeaders = makeCommand("getheaders");
    constexpr auto GetPeers = makeCommand("getpeers");
};

// ============================================
// Services a node offers
// ============================================
namespace Services
{
    constexpr uint64_t FullNode = 1;
    constexpr uint64_t Handshake = 2;
    constexpr uint64_t Ping = 4;
    constexpr uint64_t GetHeader = 8;
    constexpr uint64_t GetBlock = 16;
    constexpr uint64_t BroadcastNewBlock = 32;
    constexpr uint64_t BroadcastNewTx = 64;
    constexpr uint64_t GetMempool = 128;
    constexpr uint64_t GetHeaders = 256;
    constexpr uint64_t GetPeers = 512;
};

// ============================================
// Limit transaction and block size
// ============================================
constexpr uint32_t MAX_BLOCK_SIZE = 8 * 1024 * 1024 * 4;
constexpr uint32_t MAX_TX_SIZE = 8 * 1024 * 256;

// ============================================
// Global State
// ============================================

// Mempool
using MempoolMap = std::unordered_map<Array256_t, Tx, Array256Hash>;
inline MempoolMap mempool;

// IO context
inline asio::io_context ioCtx;

// Protocol version this node is running
constexpr uint64_t LocalProtocolVersion = 1;

// Does this node accept mempool transactions?
constexpr uint64_t RELAY = 1;

// Local nonce to ensure no self connections happen
const uint64_t LOCAL_NONCE = [] { static std::mt19937_64 g(std::random_device{}()); return g(); }();

asio::awaitable<void> acceptConnections();

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<bool> syncIfBetter(asio::ip::tcp::socket& socket);

// ============================================
// Update chain and connect to network
// ============================================

asio::awaitable<bool> trySyncWithPeers();

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(const Tx& tx);

asio::awaitable<void> BroadcastNewBlock(const ChainBlock& block);
