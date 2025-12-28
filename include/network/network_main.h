#pragma once
#include "storage/peers.h"
#include <random>
#include <unordered_set>
#include <asio/awaitable.hpp>

#include "storage/block/genesis_block.h"

// ============================================
// Data Structures
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

constexpr uint32_t MAX_BLOCK_SIZE = 8 * 1024 * 1024 * 4;
constexpr uint32_t MAX_TX_SIZE = 8 * 1024 * 256;

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

// Global State
using MempoolMap = std::unordered_map<Array256_t, Tx, Array256Hash>;
inline MempoolMap mempool;
inline asio::io_context ioCtx;

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

constexpr uint64_t ProtocolVersion = 1;
const Array256_t GenesisBlockHash = getGenesisBlockHash();

inline uint64_t generateLocalNonce()
{
        static std::mt19937_64 gen(std::random_device{}());
        return gen();
}


const uint64_t LOCAL_NONCE = generateLocalNonce();

constexpr uint64_t RELAY = 0;

asio::awaitable<void> acceptConnections();

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<bool> syncIfBetter(asio::ip::tcp::socket& socket);

// ============================================
// Update chain and connect to network
// ============================================

asio::awaitable<void> trySyncWithPeers();

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(const Tx& tx);

asio::awaitable<void> BroadcastNewBlock(const ChainBlock& block);
