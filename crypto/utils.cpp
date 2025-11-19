#pragma once
#include "utils.h"
#include "types.h"
#include <stdexcept>
#include <sodium.h>
#include <span>

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


template <typename T>
T formatNumber(std::span<const uint8_t> in) {
	if (in.size() < sizeof(T)) {
		throw std::runtime_error("formatNumber: not enough bytes in input span");
	}

	T value{};
	std::memcpy(&value, in.data(), sizeof(T));

	if constexpr (isLittleEndian()) {
		return value;
	}
	else {
		T out{};
		for (size_t i = 0; i < sizeof(T); ++i) {
			T byte = (value >> (8 * i)) & 0xFF;
			out |= (byte << (8 * (sizeof(T) - 1 - i)));
		}
		return out;
	}
}


// Serialise number to little endian
template <typename T>
requires std::is_integral_v<T>
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

		auto append = [&](auto&& data) {
			out.insert(out.end(), data.begin(), data.end());
			};

		append(txInput.UTXOTxHash);
		append(serialiseNumber(txInput.UTXOOutputIndex));
		append(txInput.signature);

		return out;
	}

	static TxInput formatTxInput(std::span<const uint8_t> txInputBytes) {
		TxInput txInput;
		size_t offset = 0;

		auto take = [&](size_t n) -> std::span<const uint8_t> {
			if (offset + n > txInputBytes.size())
				throw std::runtime_error("formatTxInput: out of range");
			auto s = txInputBytes.subspan(offset, n);
			offset += n;
			return s;
			};

		// Copy UTXO transaction hash
		auto hashBytes = take(sizeof(txInput.UTXOTxHash));
		memcpy(txInput.UTXOTxHash.data(), hashBytes.data(), hashBytes.size());

		// Read output index
		auto indexBytes = take(sizeof(txInput.UTXOOutputIndex));
		txInput.UTXOOutputIndex = formatNumber<decltype(txInput.UTXOOutputIndex)>(indexBytes.data());

		// Copy signature
		auto sigBytes = take(sizeof(txInput.signature));
		memcpy(txInput.signature.data(), sigBytes.data(), sigBytes.size());

		return txInput;
	}

	// ----------------------------------------
	// UTXO
	// ----------------------------------------
	static std::vector<uint8_t> serialiseUTXO(const UTXO& utxo) {
		std::vector<uint8_t> out;

		auto append = [&](auto&& data) {
			out.insert(out.end(), data.begin(), data.end());
			};

		append(serialiseNumber(utxo.amount));
		append(utxo.recipient);

		return out;
	}

	static UTXO formatUTXO(std::span<const uint8_t> utxoBytes) {
		UTXO utxo;
		size_t offset = 0;

		auto take = [&](size_t n) -> std::span<const uint8_t> {
			if (offset + n > utxoBytes.size())
				throw std::runtime_error("formatUTXO: out of range");
			auto s = utxoBytes.subspan(offset, n);
			offset += n;
			return s;
			};

		// Read amount
		auto amountBytes = take(sizeof(utxo.amount));
		utxo.amount = formatNumber<decltype(utxo.amount)>(amountBytes.data());

		// Copy recipient hash
		auto recipientBytes = take(sizeof(utxo.recipient));
		memcpy(utxo.recipient.data(), recipientBytes.data(), recipientBytes.size());

		return utxo;
	}

	// ----------------------------------------
	// Tx
	// ----------------------------------------
	static std::vector<uint8_t> serialiseTx(const Tx& tx) {
		std::vector<uint8_t> out;

		auto append = [&](auto&& data) {
			out.insert(out.end(), data.begin(), data.end());
			};

		// Transaction input count
		uint32_t inputCount = static_cast<uint32_t>(tx.txInputs.size());
		append(serialiseNumber(inputCount));

		// Serialize each input
		for (const auto& in : tx.txInputs) {
			append(serialiseTxInput(in));
		}

		// Transaction output count
		uint32_t outputCount = static_cast<uint32_t>(tx.txOutputs.size());
		append(serialiseNumber(outputCount));

		// Serialize each output
		for (const auto& outTx : tx.txOutputs) {
			append(serialiseUTXO(outTx));
		}

		return out;
	}


	static Tx formatTx(std::span<const uint8_t> txBytes) {
		Tx tx;
		size_t offset = 0;

		auto take = [&](size_t n) -> std::span<const uint8_t> {
			if (offset + n > txBytes.size())
				throw std::runtime_error("formatTx: out of range");
			auto s = txBytes.subspan(offset, n);
			offset += n;
			return s;
			};

		// Read input and output counts
		uint32_t inputCount = formatNumber<uint32_t>(take(sizeof(uint32_t)).data());


		// Read inputs
		for (uint32_t i = 0; i < inputCount; i++) {
			auto inputBytes = take(inputSize);  // inputSize = size of one TxInput
			tx.txInputs.push_back(formatTxInput(inputBytes));
		}

		// Read output count (after inputs)
		uint32_t outputCount = formatNumber<uint32_t>(take(sizeof(uint32_t)).data());

		// Read outputs
		for (uint32_t i = 0; i < outputCount; i++) {
			auto outputBytes = take(outputSize); // outputSize = size of one UTXO
			tx.txOutputs.push_back(formatUTXO(outputBytes));
		}

		return tx;
	}

	// ----------------------------------------
	// Block
	// ----------------------------------------

	static std::vector<uint8_t> serialiseBlock(const Block& block) {
		std::vector<uint8_t> out;

		auto append = [&](auto&& data) {
			out.insert(out.end(), data.begin(), data.end());
			};

		append(serialiseNumber(block.version));
		append(block.previousBlockHash);
		append(block.merkleRoot);
		append(serialiseNumber(block.timestamp));
		append(block.difficulty);
		append(block.nonce);

		// Transaction count
		uint32_t txCount = static_cast<uint32_t>(block.transactions.size());
		append(serialiseNumber(txCount));

		// Serialize each transaction
		for (const auto& tx : block.transactions) {
			append(serialiseTx(tx));
		}

		return out;
	}


	static Block formatBlock(std::span<const uint8_t> bytes) {
		Block block;
		size_t offset = 0;

		auto take = [&](size_t readSize) -> std::span<const uint8_t> {
			if (offset + readSize > bytes.size())
				throw std::runtime_error("block format error: out of range");
			auto s = bytes.subspan(offset, readSize);
			offset += readSize;
			return s;
			};

		// Read fields
		block.version = formatNumber<uint64_t>(take(sizeof(block.version)).data());

		std::memcpy(block.previousBlockHash.data(),
			take(block.previousBlockHash.size()).data(),
			block.previousBlockHash.size());

		std::memcpy(block.merkleRoot.data(),
			take(block.merkleRoot.size()).data(),
			block.merkleRoot.size());

		block.timestamp = formatNumber<uint64_t>(take(sizeof(block.timestamp)).data());

		std::memcpy(block.difficulty.data(),
			take(block.difficulty.size()).data(),
			block.difficulty.size());

		std::memcpy(block.nonce.data(),
			take(block.nonce.size()).data(),
			block.nonce.size());

		const uint32_t txCount = formatNumber<uint32_t>(take(sizeof(uint32_t)).data());

		// Parse transactions
		for (uint32_t i = 0; i < txCount; i++) {
			// formatTx must know how much data one transaction uses internally
			// and must throw if it's malformed
			block.transactions.push_back(formatTx(bytes.subspan(offset)));

			// Advance offset by the transaction’s size
			offset += sizeof(uint32_t) * 2 + inputSize + outputSize;
			if (offset > bytes.size())
				throw std::runtime_error("block format error: transaction out of range");
		}

		return block;
	}


	static array256_t getBlockHash(const Block& block) {
		std::vector<uint8_t> headerBytes;

		auto append = [&](auto&& data) {
			headerBytes.insert(headerBytes.end(), data.begin(), data.end());
			};

		append(serialiseNumber(block.version));
		append(block.previousBlockHash);
		append(block.merkleRoot);
		append(serialiseNumber(block.timestamp));
		append(block.difficulty);
		append(block.nonce);

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

Block formatBlock(std::span<const uint8_t> & blockBytes) {
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