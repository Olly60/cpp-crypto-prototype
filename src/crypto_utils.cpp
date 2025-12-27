#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <chrono>
#include "storage/storage_utils.h"
#include "parse_serialise.h"

// ============================================================================
// BASIC UTILITIES
// ============================================================================
namespace
{
    // Helper: convert hex character to nibble
    constexpr uint8_t hexCharToNibble(const char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("Invalid hex character");
    }
}

// Convert hex string to byte array
BytesBuffer hexToBytes(const std::string& hex)
{
    if (hex.size() % 2 != 0)
    {
        throw std::runtime_error("hexToBytes: invalid hex string length");
    }

    BytesBuffer bytes;
    for (size_t i = 0; i < hex.size() / 2; i++)
    {
        const uint8_t high = hexCharToNibble(hex[i * 2]);
        const uint8_t low = hexCharToNibble(hex[i * 2 + 1]);
        bytes.writeU8((high << 4 | low));
    }

    return bytes;
}

// Convert byte array to hex string
std::string bytesToHex(const BytesBuffer& bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);

    for (const auto& byte : bytes)
    {
        constexpr char hexChars[] = "0123456789ABCDEF";
        hex.push_back(hexChars[byte >> 4]);
        hex.push_back(hexChars[byte & 0x0F]);
    }

    return hex;
}

Array256_t sha256Of(const BytesBuffer& data)
{
    Array256_t out;
    crypto_hash_sha256(out.data(), data.data(), data.size());
    return out;
}

uint64_t getCurrentTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

// ============================================================================
// HASHING FUNCTIONS
// ============================================================================

Array256_t getBlockHeaderHash(const BlockHeader& header)
{
    return sha256Of(serialiseBlockHeader(header));
}

Array256_t getTxHash(const Tx& tx)
{
    return sha256Of(serialiseTx(tx));
}

Array256_t getMerkleRoot(const std::vector<Tx>& txs)
{
    if (txs.empty())
    {
        return Array256_t{}; // Empty merkle root for no transactions
    }

    // Build initial layer from transaction hashes
    std::vector<Array256_t> currentLayer;
    currentLayer.reserve(txs.size());
    for (const auto& tx : txs)
    {
        currentLayer.push_back(getTxHash(tx));
    }

    // Build merkle tree bottom-up
    while (currentLayer.size() > 1)
    {
        std::vector<Array256_t> nextLayer;
        nextLayer.reserve((currentLayer.size() + 1) / 2);

        for (size_t i = 0; i < currentLayer.size(); i += 2)
        {
            const Array256_t& left = currentLayer[i];
            // If odd number of elements, duplicate the last one
            const Array256_t& right = (i + 1 < currentLayer.size())
                                          ? currentLayer[i + 1]
                                          : currentLayer[i];

            // Hash the concatenation of left and right
            BytesBuffer combined;
            combined.reserve(left.size() + right.size());
            combined.writeArray256(left);
            combined.writeArray256(right);

            nextLayer.push_back(sha256Of(combined));
        }

        currentLayer = std::move(nextLayer);
    }

    return currentLayer[0];
}

// ============================================================================
// SIGNING
// ============================================================================

Array256_t computeTxSignHash(const Tx& tx, uint64_t inputIndex)
{
    BytesBuffer buf;
    buf.reserve(tx.txInputs.size() * 40 + tx.txOutputs.size() * 40 + 16);

    buf.writeU64(tx.version);

    // Inputs
    buf.writeU64(tx.txInputs.size());
    for (size_t i = 0; i < tx.txInputs.size(); ++i)
    {
        const auto& input = tx.txInputs[i];
        buf.writeArray256(input.UTXOTxHash);
        buf.writeU64(input.UTXOOutputIndex);

        // Add the input index only for the input being signed
        if (i == inputIndex)
            buf.writeU64(i);
    }

    // Outputs
    buf.writeU64(tx.txOutputs.size());
    for (const auto& [amount, recipient] : tx.txOutputs)
    {
        buf.writeU64(amount);
        buf.writeArray256(recipient);
    }

    return sha256Of(buf);
}




Tx signTxInputs(const Tx& tx, const Array256_t& privKeySeed)
{
    Tx signedTx = tx; // make a copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++)
    {
        Array256_t hash = computeTxSignHash(signedTx, i); // input-specific hash

        Array512_t sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), privKeySeed.data());

        signedTx.txInputs[i].signature = sig; // assign signature
    }

    return signedTx;
}


// ============================================================================
// BLOCK WORK
// ============================================================================

// Get Block work
Array256_t getBlockWork(const Array256_t& difficulty)
{
    Array256_t blockWork;
    blockWork.fill(0xFF);  // start with 2^256 - 1
    uint64_t shiftAmount = 0;

    // Count consecutive 1 bits in difficulty
    for (auto u8 : difficulty)
    {
        for (uint8_t mask = 1; mask != 0; mask <<= 1)
        {
            if ((u8 & mask) != 0)
                shiftAmount++;
            else
                break; // stop at first 0 if bits are always in a row
        }
    }

    // Shift work left by shiftAmount
    for (uint64_t i = 0; i < shiftAmount; ++i)
        blockWork = increaseDifficultyLE(blockWork);

    return blockWork;
}

Array256_t addBlockWorkLe(const Array256_t& a, const Array256_t& b)
{
    Array256_t result{};
    uint16_t carry = 0;

    // Iterate from least significant byte (index 0) to most significant (index 31)
    for (int i = 0; i < 32; ++i)
    {
        uint16_t sum = uint16_t{a[i]} + uint16_t{b[i]} + carry;
        result[i] = static_cast<uint8_t>(sum & 0xFF);  // keep lowest 8 bits
        carry = sum >> 8;                 // upper bits become carry
    }

    return result;
}


bool isLessLE(const Array256_t& a, const Array256_t& b)
{
    for (int i = 31; i >= 0; --i)
    {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false; // a == b
}

// Decrease difficulty (easier -> shift left)
Array256_t decreaseDifficultyLE(const Array256_t& arr)
{
    Array256_t newDifficulty = arr;
    uint8_t carry = 0;
    // Iterate from least significant byte to most significant
    for (unsigned char & i : newDifficulty) {
        uint8_t newCarry = i >> 7;
        i = (i << 1) | carry;
        carry = newCarry;
    }
    // Set least significant bit to 1 (index 0 in little-endian)
    newDifficulty[0] |= 1;
    return newDifficulty;
}

// Increase difficulty (harder -> shift right)
Array256_t increaseDifficultyLE(const Array256_t& arr)
{
    Array256_t newDifficulty = arr;
    uint8_t carry = 0;
    // Iterate from most significant byte to least significant
    for (size_t i = newDifficulty.size(); i-- > 0;) {
        uint8_t newCarry = newDifficulty[i] & 1;
        newDifficulty[i] = (newDifficulty[i] >> 1) | (carry << 7);
        carry = newCarry;
    }
    // Set least significant bit to 1 (index 0 in little-endian)
    newDifficulty[0] |= 1;
    return newDifficulty;
}


