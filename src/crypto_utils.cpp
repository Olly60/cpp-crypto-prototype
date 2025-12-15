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

std::string bytesToHex(const Array256_t& bytes)
{
    static constexpr char hexChars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);

    for (const uint8_t byte : bytes)
    {
        out.push_back(hexChars[byte >> 4]);
        out.push_back(hexChars[byte & 0x0F]);
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
std::vector<uint8_t> serialiseTxInput(const TxInput& txInput)
{
    std::vector<uint8_t> out;
    serialiseAppendBytes(out, txInput.UTXOTxHash);
    serialiseAppendBytes(out, txInput.UTXOOutputIndex);
    serialiseAppendBytes(out, txInput.signature);
    return out;
}

TxInput parseTxInput(std::span<const uint8_t> txInputBytes, size_t& offset)
{
    TxInput txInput;
    parseBytesInto(txInput.UTXOTxHash, txInputBytes, offset);
    parseBytesInto(txInput.UTXOOutputIndex, txInputBytes, offset);
    parseBytesInto(txInput.signature, txInputBytes, offset);
    return txInput;
}

// ----------------------------------------
// TxOutput
// ----------------------------------------
std::vector<uint8_t> serialiseTxOutput(const TxOutput& output)
{
    std::vector<uint8_t> out;
    serialiseAppendBytes(out, output.amount);
    serialiseAppendBytes(out, output.recipient);
    return out;
}

TxOutput parseTxOutput(std::span<const uint8_t> outputBytes, size_t& offset)
{
    TxOutput output;
    parseBytesInto(output.amount, outputBytes, offset);
    parseBytesInto(output.recipient, outputBytes, offset);
    return output;
}

// ----------------------------------------
// Tx
// ----------------------------------------
std::vector<uint8_t> serialiseTx(const Tx& tx)
{
    std::vector<uint8_t> out;

    // Version
    serialiseAppendBytes(out, tx.version);

    // Inputs
    serialiseAppendBytes(out, tx.txInputs.size());
    for (const auto& input : tx.txInputs)
    {
        serialiseAppendBytes(out, serialiseTxInput(input));
    }

    // Outputs
    serialiseAppendBytes(out, tx.txOutputs.size());
    for (const auto& output : tx.txOutputs)
    {
        serialiseAppendBytes(out, serialiseTxOutput(output));
    }

    return out;
}

Tx parseTx(std::span<const uint8_t> txBytes, size_t& offset)
{
    Tx tx;
    parseBytesInto(tx.version, txBytes, offset);

    // Read inputs
    uint64_t inputCount;
    parseBytesInto(inputCount, txBytes, offset);
    tx.txInputs.reserve(inputCount);
    for (uint64_t i = 0; i < inputCount; i++)
    {
        tx.txInputs.push_back(parseTxInput(txBytes, offset));
    }

    // Read outputs
    uint64_t outputCount;
    parseBytesInto(outputCount, txBytes, offset);
    tx.txOutputs.reserve(outputCount);
    for (uint64_t i = 0; i < outputCount; i++)
    {
        tx.txOutputs.push_back(parseTxOutput(txBytes, offset));
    }

    return tx;
}

Tx parseTx(std::span<const uint8_t> txBytes)
{
    size_t offset = 0;
    return parseTx(txBytes, offset);
}

// ----------------------------------------
// BlockHeader
// ----------------------------------------
std::vector<uint8_t> serialiseBlockHeader(const BlockHeader& header)
{
    std::vector<uint8_t> out;
    serialiseAppendBytes(out, header.version);
    serialiseAppendBytes(out, header.prevBlockHash);
    serialiseAppendBytes(out, header.merkleRoot);
    serialiseAppendBytes(out, header.timestamp);
    serialiseAppendBytes(out, header.difficulty);
    serialiseAppendBytes(out, header.nonce);
    return out;
}

BlockHeader parseBlockHeader(const std::span<const uint8_t> headerBytes)
{
    BlockHeader header;
    size_t offset = 0;
    parseBytesInto(header.version, headerBytes, offset);
    parseBytesInto(header.prevBlockHash, headerBytes, offset);
    parseBytesInto(header.merkleRoot, headerBytes, offset);
    parseBytesInto(header.timestamp, headerBytes, offset);
    parseBytesInto(header.difficulty, headerBytes, offset);
    parseBytesInto(header.nonce, headerBytes, offset);
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

