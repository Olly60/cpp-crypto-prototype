#pragma once
#include <vector>
#include <string>
#include <span>
#include <array>
#include <algorithm>

using Array256_t = std::array<uint8_t, 32>;

// Convert hex string to byte array
Array256_t hexToBytes(const std::string& hex);

// Convert byte array to hex string
std::string bytesToHex(const Array256_t& bytes);

// Compute SHA-256 hash of data
Array256_t sha256Of(std::span<const uint8_t> data);

// Get current UNIX timestamp in seconds
uint64_t getCurrentTimestamp();

// Detect endianness at compile time
static constexpr bool isLittleEndian() {
	constexpr uint16_t x = 1;
	return *reinterpret_cast<const uint8_t*>(&x) == 1;
}

// Main implementation: takes an offset
template <typename T>
static void takeBytesInto(T& out, std::span<const uint8_t> data, size_t& offset)
{
	if (offset + sizeof(T) > data.size())
		throw std::runtime_error("takeBytesInto: not enough bytes");

	std::array<uint8_t, sizeof(T)> temp{};
	std::memcpy(temp.data(), data.data() + offset, sizeof(T));

	if constexpr (std::is_integral_v<T>) {
		if (!isLittleEndian()) {
			std::reverse(temp.begin(), temp.end());
		}
	}

	std::memcpy(&out, temp.data(), sizeof(T));
	offset += sizeof(T);
}

// Overload without offset parameter: just calls the main one with offset = 0
template <typename T>
static void takeBytesInto(T& out, std::span<const uint8_t> data)
{
	size_t offset = 0;
	takeBytesInto(out, data, offset);
}

template <typename ContainerOut, typename T>
void appendBytes(ContainerOut& out, const T& data) {
	if constexpr (std::is_integral_v<T>) {
		// --- Integral type: little-endian serialization ---
		std::array<uint8_t, sizeof(T)> temp{};
		std::memcpy(temp.data(), &data, sizeof(T));
		if constexpr (!isLittleEndian()) {
			std::reverse(temp.begin(), temp.end());
		}
		out.insert(out.end(), temp.begin(), temp.end());
	}
	else if constexpr (requires { std::data(data); std::size(data); }) {
		// --- Container type: write raw bytes ---
		out.insert(out.end(), data.begin(), data.end());
	}
	else {
		static_assert(always_false<T>, "Type not supported in appendBytes");
	}
}


struct TxOutput {
	uint64_t amount{ 1 };
	Array256_t recipient{};
};
struct TxInput {
	Array256_t UTXOTxHash{};
	uint32_t UTXOOutputIndex{};
	Array256_t signature{};
};

struct Tx {
	uint32_t version{ 1 };
	std::vector<TxInput> txInputs;
	std::vector<TxOutput> txOutputs;
};

struct BlockHeader {
	uint32_t version{ 1 };
	Array256_t prevBlockHash{};
	Array256_t merkleRoot{};
	uint64_t timestamp{};
	Array256_t difficulty = {};
	Array256_t nonce{};

	BlockHeader() {
		prevBlockHash.fill(0xFF);
		difficulty.fill(0xFF);
	}
};

struct Block {
	BlockHeader header;
	std::vector<Tx> txs;
};

std::vector<uint8_t> serialiseBlock(const Block& block);
Block formatBlock(std::span<const uint8_t> blockBytes);
std::vector<uint8_t> serialiseBlockHeader(const BlockHeader& header);
BlockHeader formatBlockHeader(std::span<const uint8_t> headerBytes);
Array256_t getBlockHash(const Block& block);
Array256_t getTxHash(const Tx& tx);


