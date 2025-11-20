#pragma once
#include "crypto_utils.h"
#include "types.h"
#include <stdexcept>
#include <sodium.h>
#include <span>
#include <cstring>

array256_t bytesFromHex(const std::string& hex) {
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

std::string hexFromBytes(const array256_t& bytes) {
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
	static constexpr uint8_t inputSize = sizeof(array256_t) + sizeof(uint64_t);
	static constexpr uint8_t outputSize = sizeof(array256_t) + sizeof(uint64_t) + sizeof(array256_t);
	static constexpr uint8_t blockHeaderSize = sizeof(uint64_t) + sizeof(array256_t) + sizeof(array256_t) + sizeof(uint64_t) + sizeof(array256_t) + sizeof(array256_t);
	// ----------------------------------------
	// TxInput
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
		std::vector<uint8_t> out;
		appendBytes(out, txInput.UTXOTxHash);
		appendBytes(out, serialiseNumber(txInput.UTXOOutputIndex));
		appendBytes(out, txInput.signature);
		return out;
	}

	static TxInput formatTxInput(std::span<const uint8_t> txInputBytes) {
		TxInput txInput;
		size_t offset = 0;

		// Copy UTXO transaction hash
		auto hashBytes = takeBytes(txInputBytes, sizeof(txInput.UTXOTxHash), offset);
		std::memcpy(txInput.UTXOTxHash.data(), hashBytes.data(), hashBytes.size());

		// Read output index
		txInput.UTXOOutputIndex = formatNumber<decltype(txInput.UTXOOutputIndex)>(takeBytes(txInputBytes, sizeof(txInput.UTXOOutputIndex), offset));

		// Copy signature
		auto sigBytes = takeBytes(txInputBytes, sizeof(txInput.signature), offset);
		std::memcpy(txInput.signature.data(), sigBytes.data(), sigBytes.size());

		return txInput;
	}

	// ----------------------------------------
	// UTXO
	// ----------------------------------------
	static std::vector<uint8_t> serialiseUTXO(const UTXO& utxo) {
		std::vector<uint8_t> out;
		appendBytes(out, serialiseNumber(utxo.amount));
		appendBytes(out, utxo.recipient);
		return out;
	}

	static UTXO formatUTXO(std::span<const uint8_t> utxoBytes) {
		UTXO utxo;
		size_t offset = 0;

		// Read amount
		utxo.amount = formatNumber<decltype(utxo.amount)>(takeBytes(utxoBytes, sizeof(utxo.amount), offset));

		// Copy recipient hash
		auto recipientBytes = takeBytes(utxoBytes, sizeof(utxo.recipient), offset);
		std::memcpy(utxo.recipient.data(), recipientBytes.data(), recipientBytes.size());

		return utxo;
	}

	// ----------------------------------------
	// Tx
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTx(const Tx& tx) {
		std::vector<uint8_t> out;

		// Transaction input count
		uint32_t inputCount = static_cast<uint32_t>(tx.txInputs.size());
		appendBytes(out, serialiseNumber(inputCount));

		// Serialize each input
		for (const auto& in : tx.txInputs) {
			appendBytes(out, serialiseTxInput(in));
		}

		// Transaction output count
		uint32_t outputCount = static_cast<uint32_t>(tx.txOutputs.size());
		appendBytes(out, serialiseNumber(outputCount));

		// Serialize each output
		for (const auto& outTx : tx.txOutputs) {
			appendBytes(out, serialiseUTXO(outTx));
		}

		return out;
	}


	static Tx formatTx(std::span<const uint8_t> txBytes) {
		Tx tx;
		size_t offset = 0;

		// Read input and output counts
		uint32_t inputCount = formatNumber<uint32_t>(takeBytes(txBytes, sizeof(inputCount), offset).data());
		tx.txInputs.reserve(inputCount);

		// Read inputs
		for (uint32_t i = 0; i < inputCount; i++) {
			tx.txInputs.push_back(formatTxInput(takeBytes(txBytes, inputSize, offset)));
		}

		// Read output count (after inputs)
		uint32_t outputCount = formatNumber<uint32_t>(takeBytes(txBytes, sizeof(outputCount), offset).data());
		tx.txOutputs.reserve(outputCount);

		// Read outputs
		for (uint32_t i = 0; i < outputCount; i++) {
			tx.txOutputs.push_back(formatUTXO(takeBytes(txBytes, outputSize, offset)));
		}

		return tx;
	}

	// ----------------------------------------
	// Block
	// ----------------------------------------

	static const std::vector<uint8_t> serialiseBlock(const Block& block) {
		std::vector<uint8_t> out;

		// Header
		appendBytes(out, serialiseNumber(block.version));
		appendBytes(out, block.prevBlockHash);
		appendBytes(out, block.merkleRoot);
		appendBytes(out, serialiseNumber(block.timestamp));
		appendBytes(out, block.difficulty);
		appendBytes(out, block.nonce);

		// Transaction count
		appendBytes(out, serialiseNumber(static_cast<uint32_t>(block.transactions.size())));

		// Serialize each transaction
		for (const auto& tx : block.transactions) {
			appendBytes(out, serialiseTx(tx));
		}

		return out;
	}


	static Block formatBlock(std::span<const uint8_t> blockBytes) {
		Block block;
		size_t offset = 0;

		// Header
		block.version = formatNumber<uint64_t>(takeBytes(blockBytes, sizeof(block.version), offset).data());

		auto prevBlockHashBytes = takeBytes(blockBytes, sizeof(block.prevBlockHash), offset);
		std::memcpy(block.prevBlockHash.data(), prevBlockHashBytes.data(), prevBlockHashBytes.size());

		auto merkleRootBytes = takeBytes(blockBytes, sizeof(block.merkleRoot), offset);
		std::memcpy(block.merkleRoot.data(), merkleRootBytes.data(), merkleRootBytes.size());

		block.timestamp = formatNumber<uint64_t>(takeBytes(blockBytes, sizeof(block.timestamp), offset).data());

		auto difficultyBytes = takeBytes(blockBytes, sizeof(block.difficulty), offset);
		std::memcpy(block.difficulty.data(), difficultyBytes.data(), difficultyBytes.size());

		auto nonceBytes = takeBytes(blockBytes, sizeof(block.nonce), offset);
		std::memcpy(block.nonce.data(), nonceBytes.data(), nonceBytes.size());

		// Transactions
		const uint32_t txCount = formatNumber<uint32_t>(takeBytes(blockBytes, sizeof(txCount), offset).data());
		block.transactions.reserve(txCount);

		for (uint32_t i = 0; i < txCount; i++) {
			block.transactions.push_back(formatTx(takeBytes(blockBytes, inputSize, offset)));
		}

		return block;
	}


	static array256_t getBlockHash(const Block& block) {
		std::vector<uint8_t> headerBytes;

		appendBytes(headerBytes, serialiseNumber(block.version));
		appendBytes(headerBytes, block.prevBlockHash);
		appendBytes(headerBytes, block.merkleRoot);
		appendBytes(headerBytes, serialiseNumber(block.timestamp));
		appendBytes(headerBytes, block.difficulty);
		appendBytes(headerBytes, block.nonce);

		// Use std::span for safety
		return sha256Of(std::span<const uint8_t>(headerBytes));
	}
} // namespace v1

std::vector<uint8_t> serialiseBlock(const Block& block) {
	switch (block.version) {
	case 1: return v1::serialiseBlock(block);
	default: throw std::runtime_error("Unsupported Block version");
	}
}

Block formatBlock(std::span<const uint8_t>& blockBytes) {
	switch (formatNumber<uint64_t>(blockBytes.data())) {
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