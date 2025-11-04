#pragma once
#include <string>
#include <array>
#include "types.h"
#include <array>

using std::string;
using std::array;

// Convert hexadecimal string to byte array
static void bytesFromHex(hash256_t& out, const string& hex);

// Convert byte array to hexadecimal string
void hexFromBytes(string& out, const hash256_t& bytes, const uint64_t& size);

// SHA-256 Hashing
void sha256Of(hash256_t& out, const void* data, const uint64_t& len);

// Little-endian uint64_t to byte array
array<uint8_t, 8> putUint64LE(const uint64_t& value);

// Sterilise transaction for hashing
void hashTransaction(hash256_t& out, const Transaction& tx);