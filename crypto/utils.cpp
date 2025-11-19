#pragma once
#include "utils.h"
#include "types.h"
#include <stdexcept>
#include <sodium.h>

array256_t bytesFromHex(const std::string& hex) {
	if (hex.size() % 2 == 0) return;
	array256_t out;
	for (uint64_t i = 0; i < hex.size(); i = i + 2) {
		uint8_t high = toupper(hex[i]);
		uint8_t low = toupper(hex[i + 1]);
		if (high >= '0' && high <= '9') {
			high = high - '0';
		}
		else if (high >= 'A' && high <= 'F') {
			high = (high - 'A') + 10;
		}
		if (low >= '0' && low <= '9') {
			low = low - '0';
		}
		else if (low >= 'A' && low <= 'F') {
			low = (low - 'A') + 10;
		}
		out[i / 2] = (high << 4) | low;
	}
	return out;
}

std::string hexFromBytes(const array256_t& bytes, const size_t& len) {
	std::string out;
	out.clear();
	out.resize(len * 2);
	for (uint64_t i = 0; i < len; i++) {
		uint8_t high = bytes[i] >> 4;
		uint8_t low = bytes[i] & 0x0F;

		if (high >= 0 && high <= 9) {
			high += '0';
		}
		else if (high >= 10) high += ('A' - 10);
		if (low >= 0 && low <= 9) {
			low += '0';
		}
		else if (low >= 10) low += ('A' - 10);
		out[i * 2] = high;
		out[(i * 2) + 1] = low;
	}
	return out;
}

array256_t sha256Of(const uint8_t* data, const size_t& len) {
	array256_t out;
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
	return out;
}

// Detect endianness at compile time
static constexpr bool isLittleEndian() {
	constexpr uint16_t x = 1;
	return *reinterpret_cast<const uint8_t*>(&x) == 1;
}


// Fomat number to native endianness
template <typename T>
T formatNumber(const uint8_t* in) {
	// Create a value of type T and initialize to zero
	T value{};

	// Copy raw bytes from the input pointer into the value
	// This is safe even if the input is unaligned
	std::memcpy(&value, in, sizeof(T));

	// Check if the CPU is little-endian at compile time
	if constexpr (isLittleEndian()) {
		// If the CPU is little-endian, the byte order matches the input
		// so we can just return the value directly
		return value;
	}
	else {
		// For big-endian CPUs, we need to reverse the byte order
		T out{}; // Initialize output value

		// Loop over each byte of the original value
		for (size_t i = 0; i < sizeof(T); ++i) {
			// Extract the i-th byte from 'value'
			// (0 = least significant byte)
			T byte = (value >> (8 * i)) & 0xFF;

			// Place the extracted byte into the mirrored position in 'out'
			// sizeof(T) - 1 - i → flips the byte order:
			//   LSB moves to MSB, MSB moves to LSB
			out |= (byte << (8 * (sizeof(T) - 1 - i)));
		}

		// Return the correctly reordered number
		return out;
	}
}

// Serialise number to little endian
template <typename T>
std::array<uint8_t, sizeof(T)> serialiseNumber(const T& in) {
	// Create an array of bytes with the same size as the input type
	// '{}' ensures all bytes are initialized to 0
	std::array<uint8_t, sizeof(T)> out{};

	// Loop over each byte of the input number
	for (size_t i = 0; i < sizeof(T); i++) {
		// Shift the input 'in' right by i*8 bits to bring the i-th byte
		// into the least significant byte position
		// Then cast to uint8_t to store only that byte
		out[i] = static_cast<uint8_t>(in >> (i * 8));
	}

	// Return the array of bytes (little-endian order: LSB first)
	return out;
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
		out.insert(out.end(), txInput.UTXOTxHash.begin(), txInput.UTXOTxHash.end());
		auto serializedIndex = serialiseNumber<decltype(txInput.UTXOOutputIndex)>(txInput.UTXOOutputIndex);
		out.insert(out.end(), serializedIndex.begin(), serializedIndex.end());
		out.insert(out.end(), txInput.signature.begin(), txInput.signature.end());
		return out;
	}

	static TxInput formatTxInput(const uint8_t* txInputBytes) {
		TxInput txInput;
		memcpy(txInput.UTXOTxHash.data(), txInputBytes, sizeof(txInput.UTXOTxHash));
		txInput.UTXOOutputIndex = formatNumber<decltype(txInput.UTXOOutputIndex)>(txInputBytes + sizeof(txInput.UTXOTxHash));
		memcpy(txInput.signature.data(), txInputBytes + sizeof(txInput.UTXOOutputIndex) + sizeof(txInput.UTXOTxHash), sizeof(txInput.signature));
		return txInput;
	}

	// ----------------------------------------
	// UTXO
	// ----------------------------------------
	static std::vector<uint8_t> serialiseUTXO(const UTXO& utxo) {
		std::vector<uint8_t> out;
		auto serializedAmount = serialiseNumber(utxo.amount);
		out.insert(out.end(), serializedAmount.begin(), serializedAmount.end());
		out.insert(out.end(), utxo.recipient.begin(), utxo.recipient.end());
		return out;
	}

	static UTXO formatUTXO(const uint8_t* utxoBytes) {
		UTXO utxo;
		utxo.amount = formatNumber<decltype(utxo.amount)>(utxoBytes);
		memcpy(utxo.recipient.data(), utxoBytes + sizeof(utxo.amount), sizeof(utxo.recipient));
		return utxo;
	}

	// ----------------------------------------
	// Tx
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTx(const Tx& tx) {
		std::vector<uint8_t> out;
		std::vector<uint8_t> inputs;
		std::vector<uint8_t> outputs;
		uint32_t inputCount = 0;
		for (const TxInput& in : tx.txInputs) {
			inputCount++;
			auto v = serialiseTxInput(in);
			inputs.insert(inputs.end(), v.begin(), v.end());
		}
		uint32_t outputCount = 0;
		for (const UTXO& utxo : tx.txOutputs) {
			outputCount++;
			auto v = serialiseUTXO(utxo);
			outputs.insert(outputs.end(), v.begin(), v.end());
		}
		auto inputAmountData = serialiseNumber(inputCount);
		out.insert(out.end(), inputAmountData.begin(), inputAmountData.end());
		out.insert(out.end(), inputs.begin(), inputs.end());
		auto outputAmountData = serialiseNumber(outputCount);
		out.insert(out.end(), outputAmountData.begin(), outputAmountData.end());
		out.insert(out.end(), outputs.begin(), outputs.end());

		return out;
	}

	static Tx formatTx(const uint8_t* txBytes) {
		Tx tx;
		const uint32_t inputCount = formatNumber<uint32_t>(txBytes);
		const uint32_t outputCount = formatNumber<uint32_t>(txBytes + sizeof(inputCount) + (inputCount * inputSize));
		for (uint32_t i = 0; i < inputCount; i++) {
			tx.txInputs.push_back(
				formatTxInput(txBytes + sizeof(inputCount) + i * inputSize)
			);
		}
		for (uint32_t i = 0; i < outputCount; i++) {
			tx.txOutputs.push_back(
				formatUTXO(
					txBytes + sizeof(inputCount)
					+ sizeof(outputCount)
					+ inputCount * inputSize
					+ i * outputSize
				)
			);
		}
		return tx;
	}

	// ----------------------------------------
	// Block
	// ----------------------------------------
	static std::vector<uint8_t> serialiseBlock(const Block& block) {
		std::vector<uint8_t> out;
		auto versionBytes = serialiseNumber(block.version);
		out.insert(out.end(),versionBytes.begin(),versionBytes.end());
		out.insert(out.end(), block.previousBlockHash.begin(), block.previousBlockHash.end());
		out.insert(out.end(), block.merkleRoot.begin(), block.merkleRoot.end());
		auto timestampBytes = serialiseNumber(block.timestamp);
		out.insert(out.end(),timestampBytes.begin(),timestampBytes.end());
		out.insert(out.end(), block.difficulty.begin(), block.difficulty.end());
		out.insert(out.end(), block.nonce.begin(), block.nonce.end());

		std::vector<uint8_t> txBytes;
		uint32_t txCount = 0;

		for (const Tx& tx : block.transactions) {
			txCount++;
			std::vector<uint8_t> v = serialiseTx(tx);
			txBytes.insert(txBytes.end(), v.begin(), v.end());
		}
		auto txAmountBytes = serialiseNumber(txCount);
		out.insert(out.end(),txAmountBytes.begin(),txAmountBytes.end());
		out.insert(out.end(), txBytes.begin(), txBytes.end());
		return out;
	}

	static Block formatBlock(const uint8_t* blockBytes) {
		Block block;
		uint64_t offset = 0;
		block.version = formatNumber<decltype(block.version)>(blockBytes);
		offset += sizeof(block.version);
		memcpy(block.previousBlockHash.data(), blockBytes + offset, sizeof(block.previousBlockHash));
		offset += sizeof(block.previousBlockHash);
		memcpy(block.merkleRoot.data(), blockBytes + offset, sizeof(block.merkleRoot));
		offset += sizeof(block.merkleRoot);
		block.timestamp = formatNumber<decltype(block.timestamp)>(blockBytes + offset);
		offset += sizeof(block.timestamp);
		memcpy(block.difficulty.data(), blockBytes + offset, sizeof(block.difficulty));
		offset += sizeof(block.difficulty);
		memcpy(block.nonce.data(), blockBytes + offset, sizeof(block.nonce));
		offset += sizeof(block.nonce);
		const uint32_t txCount = formatNumber<uint32_t>(blockBytes + offset);
		offset += sizeof(txCount);
		for (uint32_t i = 0; i < txCount; i++) {
			block.transactions.push_back(formatTx(blockBytes + offset));
			offset += (sizeof(uint32_t) * 2) + formatNumber<uint32_t>(blockBytes + offset) + formatNumber<uint32_t>(blockBytes + offset + sizeof(uint32_t));
		}
		return block;
	}

	static array256_t getBlockHash(const uint8_t* blockBytes) {
		return sha256Of(blockBytes, blockHeaderSize);
	}
} // namespace v1

std::vector<uint8_t> serialiseBlock(const Block& block) {
	switch (block.version) {
	case 1: return v1::serialiseBlock(block);
	default: throw std::runtime_error("Unsupported Block version");
	}
}

Block formatBlock(const uint8_t* blockBytes) {
	switch (formatNumber<uint64_t>(blockBytes)) {
	case 1: return v1::formatBlock(blockBytes);
	default: throw std::runtime_error("Unsupported Block version");
	}
}

// Block hash from header
array256_t getBlockHash(const uint8_t* blockBytes) {
	switch (formatNumber<uint64_t>(blockBytes)) {
	case 1: return v1::getBlockHash(blockBytes);
	default: throw std::runtime_error("Unsupported Block version");
	}
}