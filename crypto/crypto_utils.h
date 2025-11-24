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

// Detect endianness at compile time
static constexpr bool isLittleEndian() {
	constexpr uint16_t x = 1;
	return *reinterpret_cast<const uint8_t*>(&x) == 1;
}

template <typename T>
static void takeBytesInto(T& out, std::span<const uint8_t> data, size_t& offset)
{
	if (offset + sizeof(T) > data.size())
		throw std::runtime_error("takeBytesInto: not enough bytes");

	std::memcpy(&out, data.data() + offset, sizeof(T));
	offset += sizeof(T);

	// If T is an integral type, convert to native endianness
	if constexpr (std::is_integral_v<T>) {
		if constexpr (!isLittleEndian()) {
			T temp = 0;
			for (size_t i = 0; i < sizeof(T); ++i) {
				T byte = (out >> (8 * i)) & 0xFF;
				temp |= (byte << (8 * (sizeof(T) - 1 - i)));
			}
			out = temp;
		}
	}
}

template <typename T>
void appendBytes(std::vector<uint8_t>& out, const T& data) {
	if constexpr (std::is_integral_v<T>) {
		// Inline little-endian serialization
		std::array<uint8_t, sizeof(T)> bytes{};
		for (size_t i = 0; i < sizeof(T); i++) {
			bytes[i] = static_cast<uint8_t>(data >> (i * 8));
		}
		out.insert(out.end(), bytes.begin(), bytes.end());
	}
	else {
		// Otherwise, assume it's a container of bytes
		out.insert(out.end(), data.begin(), data.end());
	}
}

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


