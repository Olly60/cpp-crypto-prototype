#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <chrono>

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

Array256_t hexToBytes(const std::string& hex)
{
    if (hex.size() != Array256_t{}.size() * 2)
    {
        throw std::runtime_error("hexToBytes: invalid hex string length");
    }

    Array256_t out{};
    for (size_t i = 0; i < out.size(); i++)
    {
        const uint8_t high = hexCharToNibble(hex[i * 2]);
        const uint8_t low = hexCharToNibble(hex[i * 2 + 1]);
        out[i] = (high << 4) | low;
    }

    return out;
}

Array256_t sha256Of(const std::span<const uint8_t> data)
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
// SERIALIZATION / DESERIALIZATION
// ============================================================================

// ----------------------------------------
// TxInput
// ----------------------------------------
BytesBuffer serialiseTxInput(const TxInput& txInput)
{
    return BytesBuffer() << txInput.UTXOTxHash << txInput.UTXOOutputIndex << txInput.signature;
}

TxInput parseTxInput(BytesBuffer& txInputBytes)
{
    TxInput txInput;
    txInputBytes >> txInput.UTXOTxHash >> txInput.UTXOOutputIndex >> txInput.signature;
    return txInput;
}

// ----------------------------------------
// TxOutput
// ----------------------------------------
BytesBuffer serialiseTxOutput(const TxOutput& txOutput)
{
    return BytesBuffer() << txOutput.amount << txOutput.recipient;
}

TxOutput parseTxOutput(BytesBuffer& txOutputBytes)
{
    TxOutput txOutput;
    txOutputBytes >> txOutput.amount >> txOutput.recipient;
    return txOutput;
}

// ----------------------------------------
// Tx
// ----------------------------------------
BytesBuffer serialiseTx(const Tx& tx)
{
    BytesBuffer txBytes;

    // Version
    txBytes << tx.version;

    // Inputs amount
    txBytes << tx.txInputs.size();

    // Inputs
    for (const auto& input : tx.txInputs)
    {
        txBytes << serialiseTxInput(input);
    }

    // Outputs amount
    txBytes << tx.txOutputs.size();

    // Outputs
    for (const auto& output : tx.txOutputs)
    {
        txBytes << serialiseTxOutput(output);
    }

    return txBytes;
}

Tx parseTx(BytesBuffer& txBytes)
{
    Tx tx;

    // Tx Version
    txBytes << tx.version;

    // Input amount
    uint64_t inputAmount;
    txBytes >> inputAmount;
    tx.txInputs.reserve(inputAmount);

    // Read inputs
    for (uint64_t i = 0; i < inputAmount; i++)
    {
        tx.txInputs.push_back(parseTxInput(txBytes));
    }

    // Output amount
    uint64_t outputAmount;
    txBytes >> outputAmount;
    tx.txOutputs.reserve(outputAmount);

    // Read outputs
    for (uint64_t i = 0; i < outputAmount; i++)
    {
        tx.txOutputs.push_back(parseTxOutput(txBytes));
    }

    return tx;
}

// ----------------------------------------
// BlockHeader
// ----------------------------------------
BytesBuffer serialiseBlockHeader(const BlockHeader& header)
{
    return BytesBuffer() << header.version << header.prevBlockHash << header.merkleRoot << header.timestamp << header.difficulty << header.nonce;
}

BlockHeader parseBlockHeader(BytesBuffer& headerBytes)
{
    BlockHeader header;
    headerBytes >> header.version >> header.prevBlockHash >> header.merkleRoot >> header.timestamp >> header.difficulty >> header.nonce;
    return header;
}

// ----------------------------------------
// Block
// ----------------------------------------
std::vector<uint8_t> serialiseBlock(const Block& block)
{
    std::vector<uint8_t> out;

    // Header
    serialiseAppendBytes(out, serialiseBlockHeader(block.header));

    // Transactions
    serialiseAppendBytes(out, block.txs.size());
    for (const auto& tx : block.txs)
    {
        serialiseAppendBytes(out, serialiseTx(tx));
    }

    return out;
}

Block parseBlock(const std::span<const uint8_t> blockBytes)
{
    Block block;
    size_t offset = 0;

    // Header
    block.header = parseBlockHeader(blockBytes);

    // Transactions
    uint64_t txCount;
    parseBytesInto(txCount, blockBytes, offset);
    block.txs.reserve(txCount);
    for (uint64_t i = 0; i < txCount; i++)
    {
        block.txs.push_back(parseTx(blockBytes, offset));
    }

    return block;
}

// ============================================================================
// HASHING FUNCTIONS
// ============================================================================

Array256_t getBlockHash(const Block& block)
{
    return sha256Of(serialiseBlockHeader(block.header));
}

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
            std::vector<uint8_t> combined;
            combined.reserve(left.size() + right.size());
            serialiseAppendBytes(combined, left);
            serialiseAppendBytes(combined, right);

            nextLayer.push_back(sha256Of(combined));
        }

        currentLayer = std::move(nextLayer);
    }

    return currentLayer[0];
}

// ============================================================================
// SIGNING
// ============================================================================


Array256_t computeTxInputHash(const Tx& tx)
    {
        std::vector<uint8_t> buf;

        // Version
        serialiseAppendBytes(buf, tx.version);

        // Inputs
        serialiseAppendBytes(buf, tx.txInputs.size());

        for (const auto & txInput : tx.txInputs)
        {
            serialiseAppendBytes(buf, txInput.UTXOTxHash);
            serialiseAppendBytes(buf, txInput.UTXOOutputIndex);

            // Blank out other input signatures
            std::array<uint8_t, 64> emptySig{};
            serialiseAppendBytes(buf, emptySig);
        }

        // Outputs
        serialiseAppendBytes(buf, tx.txOutputs.size());
        for (const TxOutput& txOutput : tx.txOutputs)
        {
            serialiseAppendBytes(buf, txOutput.amount);
            serialiseAppendBytes(buf, txOutput.recipient);
        }

        // Final message hash
        return sha256Of(buf);
    }

Tx signTxInputs(const Tx& tx, const Array256_t& privKeySeed)
{
    Tx signedTx = tx; // make a copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++)
    {
        Array256_t hash = computeTxInputHash(signedTx); // Sign hash for this input

        Array512_t sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), privKeySeed.data());

        signedTx.txInputs[i].signature = sig; // each input gets its own signature
    }

    return signedTx;
}

// Get Block work
Array256_t getBlockWork(BlockHeader header)
{
    // TODO: make function
    return {};
}

