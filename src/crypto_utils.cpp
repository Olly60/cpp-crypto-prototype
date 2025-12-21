#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <chrono>
// TODO: make more move semantics and improve performance
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
// SERIALIZATION / DESERIALIZATION
// ============================================================================

// ----------------------------------------
// TxInput
// ----------------------------------------
BytesBuffer serialiseTxInput(const TxInput& txInput)
{
    BytesBuffer serialisedTx;
    serialisedTx.writeArray256(txInput.UTXOTxHash);
    serialisedTx.writeU64(txInput.UTXOOutputIndex);
    serialisedTx.writeArray512(txInput.signature);
    return serialisedTx;
}

TxInput parseTxInput(BytesBuffer& txInputBytes)
{
    TxInput txInput;
    txInput.UTXOTxHash = txInputBytes.readArray256();
    txInput.UTXOOutputIndex = txInputBytes.readU64();
    txInput.signature = txInputBytes.readArray512();
    return txInput;
}

// ----------------------------------------
// TxOutput
// ----------------------------------------
BytesBuffer serialiseTxOutput(const TxOutput& txOutput)
{
    BytesBuffer serialisedTxOutput;
    serialisedTxOutput.writeU64(txOutput.amount);
    serialisedTxOutput.writeArray256(txOutput.recipient);
    return serialisedTxOutput;
}

TxOutput parseTxOutput(BytesBuffer& txOutputBytes)
{
    TxOutput txOutput;
    txOutput.amount = txOutputBytes.readU64();
    txOutput.recipient = txOutputBytes.readArray256();
    return txOutput;
}

// ----------------------------------------
// Tx
// ----------------------------------------
BytesBuffer serialiseTx(const Tx& tx)
{
    BytesBuffer txBytes;

    // Version
    txBytes.writeU64(tx.version);

    // Inputs amount
    txBytes.writeU64(tx.txInputs.size());

    // Inputs
    for (const auto& input : tx.txInputs)
    {
        txBytes.writeBytesBuffer(serialiseTxInput(input));
    }

    // Outputs amount
    txBytes.writeU64(tx.txOutputs.size());

    // Outputs
    for (const auto& output : tx.txOutputs)
    {
        txBytes.writeBytesBuffer(serialiseTxOutput(output));
    }

    return txBytes;
}

Tx parseTx(BytesBuffer& txBytes)
{
    Tx tx;

    // Tx Version
    tx.version = txBytes.readU64();

    // Input amount
    uint64_t inputAmount = txBytes.readU64();
    tx.txInputs.reserve(inputAmount);

    // Read inputs
    for (uint64_t i = 0; i < inputAmount; i++)
    {
        tx.txInputs.push_back(parseTxInput(txBytes));
    }

    // Output amount
    uint64_t outputAmount = txBytes.readU64();
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
    BytesBuffer headerBytes;
    headerBytes.writeU64(header.version);
    headerBytes.writeArray256(header.prevBlockHash);
    headerBytes.writeArray256(header.merkleRoot);
    headerBytes.writeU64(header.timestamp);
    headerBytes.writeArray256(header.difficulty);
    headerBytes.writeArray256(header.nonce);
    return headerBytes;
}

BlockHeader parseBlockHeader(BytesBuffer& headerBytes)
{
    BlockHeader header;
    header.version = headerBytes.readU64();
    header.prevBlockHash = headerBytes.readArray256();
    header.merkleRoot = headerBytes.readArray256();
    header.timestamp = headerBytes.readU64();
    header.difficulty = headerBytes.readArray256();
    header.nonce = headerBytes.readArray256();
    return header;
}

// ----------------------------------------
// Block
// ----------------------------------------
BytesBuffer serialiseBlock(const Block& block)
{

    BytesBuffer blockBytes;

    // Header
    blockBytes.writeBytesBuffer(serialiseBlockHeader(block.header));

    // Transaction amount
    blockBytes.writeU64(block.txs.size());

    // Transactions
    for (const auto& tx : block.txs)
    {
        blockBytes.writeBytesBuffer(serialiseTx(tx));
    }

    return blockBytes;
}

Block parseBlock(BytesBuffer& blockBytes)
{
    Block block;

    // Header
    block.header = parseBlockHeader(blockBytes); // TODO: append block header bytes directly to main buffer to increase performance

    // Transaction amount
    uint64_t txCount = blockBytes.readU64();
    block.txs.reserve(txCount);
    // Transactions

    for (uint64_t i = 0; i < txCount; i++)
    {
        block.txs.push_back(parseTx(blockBytes)); // TODO: like the header and transactions this will also be made directly in the main buffer
    }

    return block;
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


Array256_t computeTxInputHash(const Tx& tx)
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

