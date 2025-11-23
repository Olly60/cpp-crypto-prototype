#pragma once
#include "crypto_utils.h"
#include "types.h"
#include <stdexcept>
#include <sodium.h>
#include <span>
#include <cstring>

array256_t hexToBytes(const std::string& hex) {
	if (hex.size() != array256_t{}.size() * 2) {
		throw std::runtime_error("bytesFromHex: invalid hex string length");
	}

	array256_t out{};
	auto hexCharToNibble = [](char c) -> uint8_t {
		c = toupper(c);
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		throw std::runtime_error("bytesFromHex: invalid hex character");
		};

	for (size_t i = 0; i < out.size(); ++i) {
		uint8_t high = hexCharToNibble(hex[i * 2]);
		uint8_t low = hexCharToNibble(hex[i * 2 + 1]);
		out[i] = (high << 4) | low;
	}

	return out;
}

std::string bytesToHex(const array256_t& bytes) {
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

array256_t sha256Of(std::span<const uint8_t> data) {
	array256_t out;
	crypto_hash_sha256(out.data(), data.data(), data.size());
	return out;
}

// Detect endianness at compile time
static constexpr bool isLittleEndian() {
	constexpr uint16_t x = 1;
	return *reinterpret_cast<const uint8_t*>(&x) == 1;
}


static std::span<const uint8_t> takeBytes(std::span<const uint8_t> data, size_t amount, size_t& offset)
{
	if (offset + amount > data.size())
		throw std::runtime_error("out of range");
	auto out = data.subspan(offset, amount);
	offset += amount;
	return out;
}

template <typename Container> static void 
appendBytes(std::vector<uint8_t>& out, const Container& data) {
	out.insert(out.end(), data.begin(), data.end());
}


// ============================================================================
// v1 SERIALISERS + PARSERS
// ============================================================================
namespace v1 {

	static constexpr uint8_t inputSize = sizeof(TxInput::UTXOTxHash) + sizeof(TxInput::UTXOOutputIndex) + sizeof(TxInput::signature);
	static constexpr uint8_t outputSize = sizeof(UTXO::amount) + sizeof(UTXO::recipient);
	static constexpr uint8_t blockHeaderSize = sizeof(Block::version) + sizeof(Block::prevBlockHash) + sizeof(Block::merkleRoot) + sizeof(Block::timestamp) + sizeof(Block::difficulty) + sizeof(Block::nonce);
	// ----------------------------------------
	// TxInput
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
		std::vector<uint8_t> out;
		appendBytes(out, txInput.UTXOTxHash);
		appendBytes(out, serialiseNumberLe(txInput.UTXOOutputIndex));
		appendBytes(out, txInput.signature);
		return out;
	}

	static TxInput formatTxInput(std::span<const uint8_t> txInputBytes) {
		TxInput txInput;
		size_t offset = 0;

		// Copy UTXO transaction hash
		auto hashBytes = takeBytes(txInputBytes, sizeof(TxInput::UTXOTxHash), offset);
		std::memcpy(txInput.UTXOTxHash.data(), hashBytes.data(), hashBytes.size());

		// Read output index
		txInput.UTXOOutputIndex = formatNumberNative<decltype(txInput.UTXOOutputIndex)>(takeBytes(txInputBytes, sizeof(TxInput::UTXOOutputIndex), offset));

		// Copy signature
		auto sigBytes = takeBytes(txInputBytes, sizeof(TxInput::signature), offset);
		std::memcpy(txInput.signature.data(), sigBytes.data(), sigBytes.size());

		return txInput;
	}

	// ----------------------------------------
	// UTXO
	// ----------------------------------------
	static std::vector<uint8_t> serialiseUtxo(const UTXO& utxo) {
		std::vector<uint8_t> out;
		appendBytes(out, serialiseNumberLe(utxo.amount));
		appendBytes(out, utxo.recipient);
		return out;
	}

	static UTXO formatUtxo(std::span<const uint8_t> utxoBytes) {
		UTXO utxo;
		size_t offset = 0;

		// Read amount
		utxo.amount = formatNumberNative<decltype(utxo.amount)>(takeBytes(utxoBytes, sizeof(UTXO::amount), offset));

		// Copy recipient hash
		auto recipientBytes = takeBytes(utxoBytes, sizeof(UTXO::recipient), offset);
		std::memcpy(utxo.recipient.data(), recipientBytes.data(), recipientBytes.size());

		return utxo;
	}

	// ----------------------------------------
	// Tx
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTx(const Tx& tx) {
		std::vector<uint8_t> out;

		// Version
		appendBytes(out, serialiseNumberLe(tx.version));

		// Transaction input count
		uint32_t inputCount = static_cast<uint32_t>(tx.txInputs.size());
		appendBytes(out, serialiseNumberLe(inputCount));

		// Serialize each input
		for (const auto& in : tx.txInputs) {
			appendBytes(out, serialiseTxInput(in));
		}

		// Transaction output count
		uint32_t outputCount = static_cast<uint32_t>(tx.txOutputs.size());
		appendBytes(out, serialiseNumberLe(outputCount));

		// Serialize each output
		for (const auto& outTx : tx.txOutputs) {
			appendBytes(out, serialiseUtxo(outTx));
		}

		return out;
	}

	static Tx formatTx(std::span<const uint8_t> txBytes) {
		Tx tx;
		size_t offset = 0;

		// Version
		tx.version = formatNumberNative<uint64_t>(takeBytes(txBytes, sizeof(Tx::version), offset));

		// Read input and output counts
		uint32_t inputCount = formatNumberNative<uint32_t>(takeBytes(txBytes, sizeof(inputCount), offset));
		tx.txInputs.reserve(inputCount);

		// Read inputs
		for (uint32_t i = 0; i < inputCount; i++) {
			tx.txInputs.push_back(formatTxInput(takeBytes(txBytes, inputSize, offset)));
		}

		// Read output count (after inputs)
		uint32_t outputCount = formatNumberNative<uint32_t>(takeBytes(txBytes, sizeof(outputCount), offset));
		tx.txOutputs.reserve(outputCount);

		// Read outputs
		for (uint32_t i = 0; i < outputCount; i++) {
			tx.txOutputs.push_back(formatUtxo(takeBytes(txBytes, outputSize, offset)));
		}

		return tx;
	}

	// ----------------------------------------
	// Block
	// ----------------------------------------

	static const std::vector<uint8_t> serialiseBlock(const Block& block) {
		std::vector<uint8_t> out;

		// Header
		appendBytes(out, serialiseNumberLe(block.version));
		appendBytes(out, block.prevBlockHash);
		appendBytes(out, block.merkleRoot);
		appendBytes(out, serialiseNumberLe(block.timestamp));
		appendBytes(out, block.difficulty);
		appendBytes(out, block.nonce);

		// Transaction count
		appendBytes(out, serialiseNumberLe(static_cast<uint32_t>(block.txs.size())));

		// Serialize each transaction
		for (const auto& tx : block.txs) {
			appendBytes(out, v1::serialiseTx(tx));
		}

		return out;
	}


	static Block formatBlock(std::span<const uint8_t> blockBytes) {
		Block block;
		size_t offset = 0;

		// Header
		block.version = formatNumberNative<uint64_t>(takeBytes(blockBytes, sizeof(block.version), offset));

		auto prevBlockHashBytes = takeBytes(blockBytes, sizeof(Block::prevBlockHash), offset);
		std::memcpy(block.prevBlockHash.data(), prevBlockHashBytes.data(), prevBlockHashBytes.size());

		auto merkleRootBytes = takeBytes(blockBytes, sizeof(Block::merkleRoot), offset);
		std::memcpy(block.merkleRoot.data(), merkleRootBytes.data(), merkleRootBytes.size());

		block.timestamp = formatNumberNative<uint64_t>(takeBytes(blockBytes, sizeof(Block::timestamp), offset));

		auto difficultyBytes = takeBytes(blockBytes, sizeof(Block::difficulty), offset);
		std::memcpy(block.difficulty.data(), difficultyBytes.data(), difficultyBytes.size());

		auto nonceBytes = takeBytes(blockBytes, sizeof(Block::nonce), offset);
		std::memcpy(block.nonce.data(), nonceBytes.data(), nonceBytes.size());

		// Transactions
		const uint32_t txCount = formatNumberNative<uint32_t>(takeBytes(blockBytes, sizeof(txCount), offset));
		block.txs.reserve(txCount);

		for (uint32_t i = 0; i < txCount; i++) {
			block.txs.push_back(formatTx(takeBytes(blockBytes, inputSize, offset)));
		}

		return block;
	}


	static array256_t getBlockHash(const Block& block) {
		std::vector<uint8_t> headerBytes;

		appendBytes(headerBytes, serialiseNumberLe(block.version));
		appendBytes(headerBytes, block.prevBlockHash);
		appendBytes(headerBytes, block.merkleRoot);
		appendBytes(headerBytes, serialiseNumberLe(block.timestamp));
		appendBytes(headerBytes, block.difficulty);
		appendBytes(headerBytes, block.nonce);

		// Use std::span for safety
		return sha256Of(std::span<const uint8_t>(headerBytes));
	}

	static array256_t getTxHash(const Tx& tx) {
		std::vector<uint8_t> txBytes;

		// Serialize each input
		for (const auto& in : tx.txInputs) {
			appendBytes(txBytes, serialiseTxInput(in));
		}

		// Serialize each output
		for (const auto& outTx : tx.txOutputs) {
			appendBytes(txBytes, serialiseUtxo(outTx));
		}

		return sha256Of(std::span<const uint8_t>(txBytes));
	}
} // namespace v1

// ============================================================================
// GENERIC SERIALISERS + PARSERS
// ============================================================================

std::vector<uint8_t> serialiseTx(const Tx& tx) {
	switch (tx.version) {
	case 1: return v1::serialiseTx(tx);
	default: throw std::runtime_error("Unsupported Transaction version");
	}
}

// Serialise Block to bytes
std::vector<uint8_t> serialiseBlock(const Block& block) {
	switch (block.version) {
	case 1: return v1::serialiseBlock(block);
	default: throw std::runtime_error("Unsupported Block version");
	}
}

// Block from bytes
Block formatBlock(std::span<const uint8_t> blockBytes) {
	switch (formatNumberNative<uint64_t>(blockBytes)) {
	case 1: return v1::formatBlock(blockBytes);
	default: throw std::runtime_error("Unsupported Block version");
	}
}

// Block hash from header
array256_t getBlockHash(const Block& block) {
	switch (block.version) {
	case 1: return v1::getBlockHash(block);
	default: throw std::runtime_error("Unsupported Block version");
	}
}