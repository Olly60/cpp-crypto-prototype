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

Tx signTxInputs(const Tx& tx, const Array512_t& sk) // full secret key
{
    Tx signedTx = tx; // copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++)
    {
        Array256_t hash = computeTxSignHash(signedTx, i); // input-specific hash

        Array512_t sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), sk.data());

        signedTx.txInputs[i].signature = sig;
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
    blockWork.fill(0xFF); // 2^256

    // Count consecutive 1 bits in difficulty (MSB-first)
    size_t shiftAmount = 0;
    for (size_t i = 0; i < 32; ++i)
    {
        for (uint8_t mask = 0x80; mask != 0; mask >>= 1) // MSB → LSB in each byte
        {
            if (difficulty[i] & mask) shiftAmount++;
            else break;
        }
    }

    // Shift left by shiftAmount (easier)
    for (size_t i = 0; i < shiftAmount; ++i)
        blockWork = shiftLeft(blockWork);

    return blockWork;
}
// TODO: fix these functions


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
}


// Shift right (harder) -> divide by 2
Array256_t shiftRightBE(const Array256_t& arr)
{
    Array256_t result = arr;
    uint8_t carry = 0;

    for (size_t i = 0; i < 32; ++i) // MSB -> LSB
    {
        uint8_t newCarry = result[i] & 1;
        result[i] = (result[i] >> 1) | (carry << 7);
        carry = newCarry;
    }
    // Ensure at least 1 in LSB so it never becomes zero
    result[31] |= 1;

    return result;
}

// Shift left (easier) -> multiply by 2
Array256_t shiftLeftBE(const Array256_t& arr)
{
    Array256_t result = arr;
    uint8_t carry = 0;

    for (int i = 31; i >= 0; --i) // LSB -> MSB for carry
    {
        uint8_t newCarry = (result[i] & 0x80) >> 7;
        result[i] = (result[i] << 1) | carry;
        carry = newCarry;
    }
    // Ensure at least 1 in LSB so it never becomes zero
    result[31] |= 1;

    return result;
}


