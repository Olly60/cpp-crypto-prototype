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

// Detect endianness at compile time
static constexpr bool isLittleEndian() {
	constexpr uint16_t x = 1;
	return *reinterpret_cast<const uint8_t*>(&x) == 1;
}

// Take bytes from data span into out variable, updating offset
template <typename T>
static void takeBytesInto(T& out, std::span<const uint8_t> data, size_t& offset)
{
	// Ensure enough bytes are available
	if (offset + sizeof(T) > data.size()) throw std::runtime_error("takeBytesInto: not enough bytes");

	std::array<uint8_t, sizeof(T)> temp{};
	std::memcpy(temp.data(), data.data() + offset, sizeof(T));

	// Endianness fix for integral types
	if constexpr (std::is_integral_v<T>) {
		// If host is big-endian, swap the bytes read from little-endian storage
		if constexpr (!isLittleEndian()) {
			std::reverse(temp.begin(), temp.end());
		}
	}

	// Copy bytes into out
	std::memcpy(&out, temp.data(), sizeof(T));
	offset += sizeof(T);
}

// Overload without offset parameter (uses internal static offset)
template <typename T>
static void takeBytesInto(T& out, std::span<const uint8_t> data)
{
	// Ensure enough bytes are available
	if (offset + sizeof(T) > data.size()) throw std::runtime_error("takeBytesInto: not enough bytes");

	std::array<uint8_t, sizeof(T)> temp{};
	std::memcpy(temp.data(), data.data(), sizeof(T));

	// Endianness fix for integral types
	if constexpr (std::is_integral_v<T>) {
		// If host is big-endian, swap the bytes read from little-endian storage
		if constexpr (!isLittleEndian()) {
			std::reverse(temp.begin(), temp.end());
		}
	}

	// Copy bytes into out
	std::memcpy(&out, temp.data(), sizeof(T));
}

// Append bytes of data to output container
template <typename ContainerOut, typename T>
void appendBytes(ContainerOut out, const T& data) {

	// Inline little-endian serialization
	std::array<uint8_t, sizeof(T)> temp{};
	std::memcpy(temp.data(), &data, sizeof(T));
	// Endianness fix for integral types
	if constexpr (std::is_integral_v<T>) {
		// If host is big-endian, swap the bytes read from little-endian storage
		if constexpr (!isLittleEndian()) {
			std::reverse(temp.begin(), temp.end());
		}
	}
	out.insert(out.end(), temp.begin(), temp.end());

	// Otherwise, assume it's a container of bytes
	out.insert(out.end(), data.begin(), data.end());
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
struct Block {
	uint32_t version{ 1 };
	Array256_t prevBlockHash{};
	Array256_t merkleRoot{};
	uint64_t timestamp{};
	Array256_t difficulty{};
	Array256_t nonce{};
	std::vector<Tx> txs;
};

std::vector<uint8_t> serialiseBlock(const Block& block);
Block formatBlock(std::span<const uint8_t> blockBytes);
Array256_t getBlockHash(const Block& block);
Array256_t getTxHash(const Tx& tx);


