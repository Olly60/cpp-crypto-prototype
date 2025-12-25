#pragma once
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <istream>
#include "parse_serialise.h"


// ============================================================================
// TYPE ALIASES
// ============================================================================

using Array256_t = std::array<uint8_t, 32>;

using Array512_t = std::array<uint8_t, 64>;

// ============================================================================
// MAIN BUFFER
// ============================================================================

// Convert 32-byte array to hex string
std::string bytesToHex(const BytesBuffer& bytes);

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct TxOutput
{
    uint64_t amount = 0;
    Array256_t recipient{};
};

struct TxInput
{
    Array256_t UTXOTxHash{}; // Hash of transaction containing the UTXO
    uint64_t UTXOOutputIndex = 0; // Index of output in that transaction
    Array512_t signature{}; // Signature proving ownership
};

struct Tx
{
    uint64_t version = 1;
    std::vector<TxInput> txInputs{};
    std::vector<TxOutput> txOutputs{};
};

struct BlockHeader
{
    uint64_t version = 1;
    Array256_t prevBlockHash{};
    Array256_t merkleRoot{};
    uint64_t timestamp = 0;
    Array256_t difficulty{};
    Array256_t nonce{};

    BlockHeader()
    {
        prevBlockHash.fill(0xFF);
        difficulty.fill(0xFF);
    }
};

struct Block
{
    BlockHeader header;
    std::vector<Tx> txs{};
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

constexpr size_t calculateBlockHeaderSize()
{
    return sizeof(decltype(BlockHeader::version)) // version
        + sizeof(decltype(BlockHeader::prevBlockHash)) // prevBlockHash
        + sizeof(decltype(BlockHeader::merkleRoot)) // merkleRoot
        + sizeof(decltype(BlockHeader::timestamp)) // timestamp
        + sizeof(decltype(BlockHeader::difficulty)) // difficulty
        + sizeof(decltype(BlockHeader::nonce)); // nonce
}

// Convert hex string to 32-byte array
BytesBuffer hexToBytes(const std::string& hex);


// Compute SHA-256 hash of data
Array256_t sha256Of(const BytesBuffer& data);

// Get current UNIX timestamp in seconds
uint64_t getCurrentTimestamp();

// ============================================================================
// HASHING FUNCTIONS
// ============================================================================

// Get hash of a block header
Array256_t getBlockHeaderHash(const BlockHeader& header);

// Get hash of a transaction
Array256_t getTxHash(const Tx& tx);

// Compute merkle root from list of transactions
Array256_t getMerkleRoot(const std::vector<Tx>& txs);

// ============================================================================
// SIGNING
// ============================================================================

Array256_t computeTxHash(const Tx& tx, uint64_t inputIndex);

Tx signTxInputs(const Tx& tx, const Array256_t& privKeySeed);

// ============================================================================
// BLOCK WORK
// ============================================================================

// Get block work
Array256_t getBlockWork(const Array256_t& difficulty);

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b);

// Returns true if a > b
bool isLessLE(const Array256_t& a, const Array256_t& b);

// Decrease difficulty (easier -> shift left)
Array256_t decreaseDifficulty(const Array256_t& arr);

// Increase difficulty (harder -> shift right)
Array256_t increaseDifficulty(const Array256_t& arr);
