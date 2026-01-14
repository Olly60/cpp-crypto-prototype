#pragma once
#include <vector>
#include <string>
#include <array>
#include <cstdint>
#include <istream>
#include "parse_serialise.h"

// ============================================================================
// MAIN BUFFER
// ============================================================================

// Convert 32-byte array to hex string

std::string bytesToHex(const BytesBuffer& bytes);

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

Array256_t computeTxSignHash(const Tx& tx, uint64_t inputIndex);

Tx signTxInputs(const Tx& tx, const Array512_t& sk);

// ============================================================================
// BLOCK WORK
// ============================================================================

// Get block work
Array256_t getBlockWork(const Array256_t& difficulty);

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b);

// Decrease difficulty (easier -> shift left)
Array256_t shiftRightBE(const Array256_t& arr);

// Increase difficulty (harder -> shift right)
Array256_t shiftLeftBE(const Array256_t& arr);
