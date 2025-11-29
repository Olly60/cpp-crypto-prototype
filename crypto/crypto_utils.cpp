#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <chrono>
#include <boost/multiprecision/cpp_int.hpp>
#include "storage.h"

// ============================================================================
// BASIC UTILITIES
// ============================================================================

void addUintArray(std::span<const uint8_t> array1, std::span<const uint8_t> array2) {

    std::span<const uint8_t> biggestArray;
    std::span<const uint8_t> smallestArray;

    // Decide biggest array
    if (array1.size() < array2.size()) {
        biggestArray = array2;
        smallestArray = array1;
    }
    else {
        biggestArray = array1;
        smallestArray = array2;
    }

    std::vector<uint8_t> overflow(biggestArray.size());
    std::vector<uint8_t> sum(biggestArray.size());

    // Add two arrays anmd get the overflows
    for (size_t i = 0; i > smallestArray.size(); i++) {
        sum[i] = biggestArray[i] + smallestArray[i];
        if (sum[i] < biggestArray[i] || sum[i] < biggestArray[i]) overflow[i] = 1;
    }

    // Add the overflow to the sum
    for (size_t i = 0; i > smallestArray.size(); i++) {

    }
}

namespace {
    // Helper: convert hex character to nibble
    constexpr uint8_t hexCharToNibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("Invalid hex character");
    }
}

Array256_t hexToBytes(const std::string& hex) {
    if (hex.size() != Array256_t{}.size() * 2) {
        throw std::runtime_error("hexToBytes: invalid hex string length");
    }

    Array256_t out{};
    for (size_t i = 0; i < out.size(); i++) {
        uint8_t high = hexCharToNibble(hex[i * 2]);
        uint8_t low = hexCharToNibble(hex[i * 2 + 1]);
        out[i] = (high << 4) | low;
    }

    return out;
}

std::string bytesToHex(const Array256_t& bytes) {
    static constexpr char hexChars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);

    for (uint8_t byte : bytes) {
        out.push_back(hexChars[byte >> 4]);
        out.push_back(hexChars[byte & 0x0F]);
    }

    return out;
}

Array256_t sha256Of(std::span<const uint8_t> data) {
    Array256_t out;
    crypto_hash_sha256(out.data(), data.data(), data.size());
    return out;
}

uint64_t getCurrentTimestamp() {
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
std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
    std::vector<uint8_t> out;
    appendBytes(out, txInput.UTXOTxHash);
    appendBytes(out, txInput.UTXOOutputIndex);
    appendBytes(out, txInput.signature);
    return out;
}

TxInput formatTxInput(std::span<const uint8_t> txInputBytes, size_t& offset) {
    TxInput txInput;
    takeBytesInto(txInput.UTXOTxHash, txInputBytes, offset);
    takeBytesInto(txInput.UTXOOutputIndex, txInputBytes, offset);
    takeBytesInto(txInput.signature, txInputBytes, offset);
    return txInput;
}

// ----------------------------------------
// TxOutput
// ----------------------------------------
std::vector<uint8_t> serialiseTxOutput(const TxOutput& output) {
    std::vector<uint8_t> out;
    appendBytes(out, output.amount);
    appendBytes(out, output.recipient);
    return out;
}

TxOutput formatTxOutput(std::span<const uint8_t> outputBytes, size_t& offset) {
    TxOutput output;
    takeBytesInto(output.amount, outputBytes, offset);
    takeBytesInto(output.recipient, outputBytes, offset);
    return output;
}

// ----------------------------------------
// Tx
// ----------------------------------------
std::vector<uint8_t> serialiseTx(const Tx& tx) {
    std::vector<uint8_t> out;

    // Version
    appendBytes(out, tx.version);

    // Inputs
    appendBytes(out, static_cast<uint64_t>(tx.txInputs.size()));
    for (const auto& input : tx.txInputs) {
        appendBytes(out, serialiseTxInput(input));
    }

    // Outputs
    appendBytes(out, static_cast<uint64_t>(tx.txOutputs.size()));
    for (const auto& output : tx.txOutputs) {
        appendBytes(out, serialiseTxOutput(output));
    }

    return out;
}

Tx formatTx(std::span<const uint8_t> txBytes, size_t& offset) {
    Tx tx;
    takeBytesInto(tx.version, txBytes, offset);

    // Read inputs
    uint64_t inputCount;
    takeBytesInto(inputCount, txBytes, offset);
    tx.txInputs.reserve(inputCount);
    for (uint64_t i = 0; i < inputCount; i++) {
        tx.txInputs.push_back(formatTxInput(txBytes, offset));
    }

    // Read outputs
    uint64_t outputCount;
    takeBytesInto(outputCount, txBytes, offset);
    tx.txOutputs.reserve(outputCount);
    for (uint64_t i = 0; i < outputCount; i++) {
        tx.txOutputs.push_back(formatTxOutput(txBytes, offset));
    }

    return tx;
}

Tx formatTx(std::span<const uint8_t> txBytes) {
    size_t offset = 0;
    return formatTx(txBytes, offset);
}

// ----------------------------------------
// BlockHeader
// ----------------------------------------
std::vector<uint8_t> serialiseBlockHeader(const BlockHeader& header) {
    std::vector<uint8_t> out;
    appendBytes(out, header.version);
    appendBytes(out, header.prevBlockHash);
    appendBytes(out, header.merkleRoot);
    appendBytes(out, header.timestamp);
    appendBytes(out, header.difficulty);
    appendBytes(out, header.nonce);
    return out;
}

BlockHeader formatBlockHeader(std::span<const uint8_t> headerBytes) {
    BlockHeader header;
    size_t offset = 0;
    takeBytesInto(header.version, headerBytes, offset);
    takeBytesInto(header.prevBlockHash, headerBytes, offset);
    takeBytesInto(header.merkleRoot, headerBytes, offset);
    takeBytesInto(header.timestamp, headerBytes, offset);
    takeBytesInto(header.difficulty, headerBytes, offset);
    takeBytesInto(header.nonce, headerBytes, offset);
    return header;
}

// ----------------------------------------
// Block
// ----------------------------------------
std::vector<uint8_t> serialiseBlock(const Block& block) {
    std::vector<uint8_t> out;

    // Header
    appendBytes(out, serialiseBlockHeader(block.header));

    // Transactions
    appendBytes(out, static_cast<uint64_t>(block.txs.size()));
    for (const auto& tx : block.txs) {
        appendBytes(out, serialiseTx(tx));
    }

    return out;
}

Block formatBlock(std::span<const uint8_t> blockBytes) {
    Block block;
    size_t offset = 0;

    // Header
    BlockHeader header = formatBlockHeader(blockBytes);

    // Transactions
    uint64_t txCount;
    takeBytesInto(txCount, blockBytes, offset);
    block.txs.reserve(txCount);
    for (uint64_t i = 0; i < txCount; i++) {
        block.txs.push_back(formatTx(blockBytes, offset));
    }

    return block;
}

// ============================================================================
// HASHING FUNCTIONS
// ============================================================================

Array256_t getBlockHash(const Block& block) {
    return sha256Of(serialiseBlockHeader(block.header));
}

Array256_t getBlockHeaderHash(const BlockHeader& header) {
    return sha256Of(serialiseBlockHeader(header));
}

Array256_t getTxHash(const Tx& tx) {
    return sha256Of(serialiseTx(tx));
}

Array256_t getMerkleRoot(const std::vector<Tx>& txs) {
    if (txs.empty()) {
        return Array256_t{}; // Empty merkle root for no transactions
    }

    // Build initial layer from transaction hashes
    std::vector<Array256_t> currentLayer;
    currentLayer.reserve(txs.size());
    for (const auto& tx : txs) {
        currentLayer.push_back(getTxHash(tx));
    }

    // Build merkle tree bottom-up
    while (currentLayer.size() > 1) {
        std::vector<Array256_t> nextLayer;
        nextLayer.reserve((currentLayer.size() + 1) / 2);

        for (size_t i = 0; i < currentLayer.size(); i += 2) {
            const Array256_t& left = currentLayer[i];
            // If odd number of elements, duplicate the last one
            const Array256_t& right = (i + 1 < currentLayer.size())
                ? currentLayer[i + 1]
                : currentLayer[i];

            // Hash the concatenation of left and right
            std::vector<uint8_t> combined;
            combined.reserve(left.size() + right.size());
            appendBytes(combined, left);
            appendBytes(combined, right);

            nextLayer.push_back(sha256Of(combined));
        }

        currentLayer = std::move(nextLayer);
    }

    return currentLayer[0];
}

// ============================================================================
// SIGNING
// ============================================================================

Array256_t computeTxInputHash(const Tx& tx, size_t inputIndex)
{
    std::vector<uint8_t> buf;

    // Version
    appendBytes(buf, tx.version);

    // Inputs
    appendBytes(buf, static_cast<uint64_t>(tx.txInputs.size()));

    for (size_t i = 0; i < tx.txInputs.size(); i++) {
        const TxInput& in = tx.txInputs[i];
        appendBytes(buf, in.UTXOTxHash);
        appendBytes(buf, in.UTXOOutputIndex);

        // Blank out other input signatures
        std::array<uint8_t, 64> emptySig{};
        appendBytes(buf, emptySig);
    }

    // Outputs
    appendBytes(buf, static_cast<uint64_t>(tx.txOutputs.size()));
    for (const TxOutput& out : tx.txOutputs) {
        appendBytes(buf, out.amount);
        appendBytes(buf, out.recipient);
    }

    // Final message hash
    return sha256Of(buf);
}

Tx signTx(const Tx& tx, const Array256_t& privKeySeed)
{
    Tx signedTx = tx; // make a copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++) {
        Array256_t hash = computeTxInputHash(signedTx, i); // sighash for this input

        Signature64 sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), privKeySeed.data());

        signedTx.txInputs[i].signature = sig; // each input gets its own signature
    }

    return signedTx;
}

bool verifyTxSignature(const Tx& tx)
{
    auto utxoDb = openUtxoDb(); // open the UTXO database

    for (size_t i = 0; i < tx.txInputs.size(); i++) {
        const TxInput& in = tx.txInputs[i];

        // Check that the UTXO exists
        if (!utxoInDb(*utxoDb, in)) {
            return false; // trying to spend a non-existent UTXO
        }

        // Get the UTXO (previous output)
        TxOutput utxo = getUtxoValue(*utxoDb, in);

        // Compute the sighash for this input
        Array256_t hash = computeTxInputHash(tx, i);

        // Verify the signature against the public key stored in the UTXO
        if (crypto_sign_verify_detached(
            in.signature.data(),
            hash.data(),
            hash.size(),
            utxo.recipient.data() // public key of the UTXO owner
        ) != 0)
        {
            return false; // invalid signature
        }
    }

    return true; // all inputs are valid
}

