#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <span>

array256_t bytesFromHex(const std::string& hex);

std::string hexFromBytes(const array256_t& bytes);

array256_t sha256Of(std::span<const uint8_t> data);

static std::span<const uint8_t> takeBytes(std::span<const uint8_t> data, size_t amount, size_t& offset);
// Format bumber to native endianness
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
template <typename T> requires std::is_integral_v<T>
std::array<uint8_t, sizeof(T)> serialiseNumber(const T in) {
	std::array<uint8_t, sizeof(T)> out{};

	// Loop over each byte of the input number
	for (size_t i = 0; i < sizeof(T); i++) {
		out[i] = static_cast<uint8_t>(in >> (i * 8));
	}

	// Return the array of bytes (little-endian order: LSB first)
	return out;
}

std::vector<uint8_t> serialiseBlock(const Block& block);

Block formatBlock(std::span<const uint8_t>& blockBytes);

array256_t getBlockHash(const Block& block);
