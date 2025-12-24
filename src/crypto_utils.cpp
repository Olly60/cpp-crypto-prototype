#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <chrono>
#include "storage/file_utils.h"
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

Array256_t computeTxHash(const Tx& tx)
{
    BytesBuffer buf;
    buf.reserve(tx.txInputs.size() * 40 + tx.txOutputs.size() * 40 + 16);

    // Version
    buf.writeU64(tx.version);

    // Input amount
    buf.writeU64(tx.txInputs.size());

        // Inputs
    for (const auto& [UTXOTxHash, UTXOOutputIndex, Signature] : tx.txInputs) {
        buf.writeArray256(UTXOTxHash);
        buf.writeU64(UTXOOutputIndex);
    }

        // Output amount
        buf.writeU64(tx.txOutputs.size());

        // Outputs
        for (const auto & [amount, recipient] : tx.txOutputs)
        {
            buf.writeU64(amount);
            buf.writeArray256(recipient);
        }

        // Final message hash
        return sha256Of(buf);
    }

Tx signTxInputs(const Tx& tx, const Array256_t& privKeySeed)
{
    Tx signedTx = tx; // make a copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++)
    {
        Array256_t hash = computeTxHash(signedTx); // Sign hash for this input

        Array512_t sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), privKeySeed.data());

        signedTx.txInputs[i].signature = sig; // each input gets its own signature
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
        blockWork = increaseDifficulty(blockWork);

    return blockWork;
}

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b)
{
    Array256_t result{};
    uint16_t carry = 0;  // allow overflow beyond 8 bits

    // Iterate from least significant byte to most significant
    for (int i = 31; i >= 0; --i)
    {
        uint16_t sum = uint16_t{a[i]} + uint16_t{b[i]} + carry;
        result[i] = uint8_t{static_cast<unsigned char>(sum & 0xFF)};  // keep lowest 8 bits
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
Array256_t decreaseDifficulty(const Array256_t& arr)
{
    Array256_t newDifficulty = arr;
    uint8_t carry = 0;
    for (auto& u8: newDifficulty) {
        uint8_t newCarry = u8 >> 7;
        u8 = (u8 << 1) | carry;
        carry = newCarry;
    }
    // Set end bit to 1
    newDifficulty.back() |= 1;
    return newDifficulty;
}

// Increase difficulty (harder -> shift right)
Array256_t increaseDifficulty(const Array256_t& arr)
{
    Array256_t newDifficulty = arr;
    uint8_t carry = 0;
    for (size_t i = newDifficulty.size(); i-- > 0;) {
        uint8_t newCarry = newDifficulty[i] & 1;
        newDifficulty[i] = (newDifficulty[i] >> 1) | (carry << 7);
        carry = newCarry;
    }
    // Set end bit to 1 (minimum difficulty)
    newDifficulty.back() |= 1;
    return newDifficulty;
}

