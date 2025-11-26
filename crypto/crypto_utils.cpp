#pragma once
#include "crypto_utils.h"
#include <stdexcept>
#include <sodium.h>
#include <cstring>
#include <chrono>

// ============================================================================
// BASIC UTILITIES
// ============================================================================
Array256_t hexToBytes(const std::string& hex) {
	if (hex.size() != Array256_t{}.size() * 2) {
		throw std::runtime_error("bytesFromHex: invalid hex string length");
	}

	Array256_t out{};
	auto hexCharToNibble = [](char c) -> uint8_t {
		c = toupper(c);
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		throw std::runtime_error("bytesFromHex: invalid hex character");
		};

	for (size_t i = 0; i < out.size(); i++) {
		uint8_t high = hexCharToNibble(hex[i * 2]);
		uint8_t low = hexCharToNibble(hex[i * 2 + 1]);
		out[i] = (high << 4) | low;
	}

	return out;
}

std::string bytesToHex(const Array256_t& bytes) {
	static const char hexChars[] = "0123456789ABCDEF";
	std::string out;
	out.resize(bytes.size() * 2);

	for (size_t i = 0; i < bytes.size(); i++) {
		uint8_t b = bytes[i];
		out[i * 2] = hexChars[b >> 4];  // high nibble
		out[i * 2 + 1] = hexChars[b & 0x0F]; // low nibble
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

// ----------------------------------------
// TxInput
// ----------------------------------------
static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
	std::vector<uint8_t> out;
	appendBytes(out, txInput.UTXOTxHash);
	appendBytes(out, txInput.UTXOOutputIndex);
	appendBytes(out, txInput.signature);
	return out;
}

static TxInput formatTxInput(std::span<const uint8_t> txInputBytes, size_t& offset) {
	TxInput txInput;
	takeBytesInto(txInput.UTXOTxHash, txInputBytes, offset);
	takeBytesInto(txInput.UTXOOutputIndex, txInputBytes, offset);
	takeBytesInto(txInput.signature, txInputBytes, offset);
	return txInput;
}

// ----------------------------------------
// UTXO
// ----------------------------------------
static std::vector<uint8_t> serialiseUtxo(const TxOutput& utxo) {
	std::vector<uint8_t> out;
	appendBytes(out, utxo.amount);
	appendBytes(out, utxo.recipient);
	return out;
}

static TxOutput formatUtxo(std::span<const uint8_t> utxoBytes, size_t& offset) {
	TxOutput utxo;
	takeBytesInto(utxo.amount, utxoBytes, offset);
	takeBytesInto(utxo.recipient, utxoBytes, offset);
	return utxo;
}

// ----------------------------------------
// Tx
// ----------------------------------------
static std::vector<uint8_t> serialiseTx(const Tx& tx) {
	std::vector<uint8_t> out;
	// Version
	appendBytes(out, tx.version);

	// Input count
	appendBytes(out, static_cast<uint32_t>(tx.txInputs.size()));
	// Serialize each input
	for (const auto& in : tx.txInputs) appendBytes(out, serialiseTxInput(in));

	// Output count
	appendBytes(out, static_cast<uint32_t>(tx.txOutputs.size()));
	// Serialize each output
	for (const auto& outTx : tx.txOutputs) appendBytes(out, serialiseUtxo(outTx));
	return out;
}

static Tx formatTx(std::span<const uint8_t> txBytes, size_t& offset) {
	Tx tx;
	takeBytesInto(tx.version, txBytes, offset);

	// Read input and output counts
	uint32_t inputCount;
	takeBytesInto(inputCount, txBytes, offset);
	tx.txInputs.reserve(inputCount);

	// Read inputs
	for (uint32_t i = 0; i < inputCount; i++) {
		tx.txInputs.push_back(formatTxInput(txBytes, offset));
	}

	// Read output count (after inputs)
	uint32_t outputCount;
	takeBytesInto(outputCount, txBytes, offset);
	tx.txOutputs.reserve(outputCount);

	// Read outputs
	for (uint32_t i = 0; i < outputCount; i++) {
		tx.txOutputs.push_back(formatUtxo(txBytes, offset));
	}
}

// ----------------------------------------
// Block
// ----------------------------------------

std::vector<uint8_t> serialiseBlock(const Block& block) {
	std::vector<uint8_t> out;
	// Header
	appendBytes(out, block.header.version);
	appendBytes(out, block.header.prevBlockHash);
	appendBytes(out, block.header.merkleRoot);
	appendBytes(out, block.header.timestamp);
	appendBytes(out, block.header.difficulty);
	appendBytes(out, block.header.nonce);
	// Transaction count
	appendBytes(out, static_cast<uint32_t>(block.txs.size()));
	// Serialize each transaction
	for (const auto& tx : block.txs) appendBytes(out, serialiseTx(tx));
	return out;
}

Block formatBlock(std::span<const uint8_t> blockBytes) {
	Block block;
	size_t offset = 0;

	// Header
	takeBytesInto(block.header.version, blockBytes, offset);
	takeBytesInto(block.header.prevBlockHash, blockBytes, offset);
	takeBytesInto(block.header.merkleRoot, blockBytes, offset);
	takeBytesInto(block.header.timestamp, blockBytes, offset);
	takeBytesInto(block.header.difficulty, blockBytes, offset);
	takeBytesInto(block.header.nonce, blockBytes, offset);

	// Transactions
	uint32_t txCount = 0;
	takeBytesInto(txCount, blockBytes, offset);
	block.txs.reserve(txCount);

	for (uint32_t i = 0; i < txCount; i++) {
		block.txs.push_back(formatTx(blockBytes, offset));
	}

	return block;
}

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

// ===========================================================
// Hashing functions
// ===========================================================

Array256_t getBlockHash(const Block& block) {
	std::vector<uint8_t> headerBytes;
	appendBytes(headerBytes, block.header.version);
	appendBytes(headerBytes, block.header.prevBlockHash);
	appendBytes(headerBytes, block.header.merkleRoot);
	appendBytes(headerBytes, block.header.timestamp);
	appendBytes(headerBytes, block.header.difficulty);
	appendBytes(headerBytes, block.header.nonce);
	return sha256Of(headerBytes);
}

Array256_t getTxHash(const Tx& tx) {
	std::vector<uint8_t> txBytes;

	// Version
	appendBytes(txBytes, tx.version);

	// Serialize each input
	for (const auto& in : tx.txInputs) appendBytes(txBytes, in);

	// Serialize each output
	for (const auto& outTx : tx.txOutputs) appendBytes(txBytes, outTx);

	return sha256Of(txBytes);
}

Array256_t getMerkleRoot(const std::vector<Tx>& txs) {
	if (txs.empty()) {
		return Array256_t{}; // empty tree
	}

	// Step 1: compute leaf hashes (hash of each transaction)
	std::vector<Array256_t> layer;
	layer.reserve(txs.size());
	for (const auto& tx : txs) {
		layer.push_back(getTxHash(tx));
	}

	// Step 2: build the Merkle tree
	while (layer.size() > 1) {
		std::vector<Array256_t> nextLayer;
		for (size_t i = 0; i < layer.size(); i += 2) {
			Array256_t left = layer[i];
			Array256_t right = (i + 1 < layer.size()) ? layer[i + 1] : layer[i]; // duplicate last if odd

			// Combine left and right hashes
			std::vector<uint8_t> combined;
			appendBytes(combined, left);
			appendBytes(combined, right);

			// Hash the pair and push to next layer
			nextLayer.push_back(sha256Of(combined));
		}
		layer = std::move(nextLayer);
	}

	return layer[0]; // final Merkle root
}