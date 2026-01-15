#pragma once
#include "storage/peers.h"
#include <random>
#include <unordered_set>
#include <asio/awaitable.hpp>
#include "storage/block/genesis_block.h"

constexpr uint64_t MAX_BLOCK_SIZE = 8 * 1024 * 1024 * 4;
constexpr uint64_t MAX_TX_SIZE = 8 * 1024 * 256;

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
    constexpr uint64_t GetBlock = 2;
    constexpr uint64_t GetMempool = 4;
    constexpr uint64_t GetHeaders = 8;
    constexpr uint64_t GetPeers = 16;
};

asio::awaitable<void> acceptConnections(uint16_t port);

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

asio::awaitable<void> BroadcastNewTx(asio::io_context &io, const Tx& tx);

asio::awaitable<void> BroadcastNewBlock(asio::io_context& io, const ChainBlock& block);
