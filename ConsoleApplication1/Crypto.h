#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "types.h"

void sha256Of(hash256_t& out, const void* data, const uint64_t& len);
void putUint64LE(uint8_t* buf, const uint64_t& value);
uint64_t getUint64LE(const uint8_t* buf);
void bytesFromHex(hash256_t& out, const std::string& hex);
void hexFromBytes(std::string& out, const hash256_t& bytes, const uint64_t& size);
