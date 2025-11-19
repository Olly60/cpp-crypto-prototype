#pragma once
#include "utils.h"
#include "types.h"
#include <vector>
#include <array>
#include <string>

array256_t bytesFromHex(const std::string& hex);

std::string hexFromBytes(const array256_t& bytes, const size_t& len);

array256_t sha256Of(const uint8_t* data, const size_t& len);

// Format number to native from little endian
template <typename T>
T formatNumber(const uint8_t* in);

// Serialise number to little endian
template <typename T>
std::array<uint8_t, sizeof(T)> serialiseNumber(const T& in);

std::vector<uint8_t> serialiseBlock(const Block& block);

Block formatBlock(const uint8_t* data);

array256_t getBlockHash(const uint8_t* blockBytes);
