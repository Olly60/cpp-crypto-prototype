#pragma once
#include <vector>
#include <string>
#include <span>
#include <array>

using array256_t = std::array<uint8_t, 32>;

// Convert hex string to byte array
array256_t hexToBytes(const std::string& hex);

// Convert byte array to hex string
std::string bytesToHex(const array256_t& bytes);

// Compute SHA-256 hash of data
array256_t sha256Of(std::span<const uint8_t> data);

// Format number to native endianness
template <typename T> requires (std::is_integral_v<T>&& std::is_trivially_copyable_v<T>)
T formatNumberNative(std::span<const uint8_t> in) {
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
template <typename T> requires std::is_integral_v<T>
std::array<uint8_t, sizeof(T)> serialiseNumberLe(const T in) {
	std::array<uint8_t, sizeof(T)> out{};

	// Loop over each byte of the input number
	for (size_t i = 0; i < sizeof(T); i++) {
		out[i] = static_cast<uint8_t>(in >> (i * 8));
	}

	// Return the array of bytes (little-endian order: LSB first)
	return out;
}

namespace block_v1 {
	struct UTXO {
		uint64_t amount{ 1 };
		array256_t recipient{};
	};
	struct TxInput {
		array256_t UTXOTxHash{};
		uint32_t UTXOOutputIndex{};
		array256_t signature{};
	};
	struct Tx {
		uint32_t version{ 1 };
		std::vector<TxInput> txInputs;
		std::vector<UTXO> txOutputs;
	};
	struct Block {
		uint32_t version{ 1 };
		array256_t prevBlockHash{};
		array256_t merkleRoot{};
		uint64_t timestamp{};
		array256_t difficulty{};
		array256_t nonce{};
		std::vector<Tx> txs;
	};
	std::vector<uint8_t> serialiseBlock(const Block& block);
	Block formatBlock(std::span<const uint8_t> blockBytes);
	array256_t getBlockHash(const Block& block);
	array256_t getTxHash(const Tx& tx);
}

