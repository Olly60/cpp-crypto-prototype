#pragma once
#include "storage/peers.h"
#include <random>
#include "storage/block/genesis_block.h"

// ============================================
// Data Structures
// ============================================

struct Handshake
{
    uint64_t version;
    Array256_t genesisBlockHash;
    uint64_t services;
    uint64_t nonce;
    Array256_t blockchainTip;
};

namespace ProtocolMessage
{
    constexpr uint8_t Handshake = 1;
    constexpr uint8_t Ping = 2;
    constexpr uint8_t GetHeader = 3;
    constexpr uint8_t GetBlock = 4;
    constexpr uint8_t BroadcastNewBlock = 5;
    constexpr uint8_t BroadcastNewTx = 6;
    constexpr uint8_t GetMempool = 7;
    constexpr uint8_t GetHeaders = 8;
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

// ============================================
// Global State
// ============================================

inline std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
inline std::unordered_map<Array256_t, Tx, Array256Hash> mempool;

enum class Service : uint64_t
{
    FullNode = 1 << 0
};

constexpr uint64_t FullNode =
    static_cast<uint64_t>(Service::FullNode);

constexpr uint64_t ProtocolVersion = 1;
const Array256_t GenesisBlockHash = getGenesisBlockHash();


inline uint64_t generateLocalNonce()
{
        static std::mt19937_64 gen(std::random_device{}());
        return gen();
}


const uint64_t LOCAL_NONCE = generateLocalNonce();
