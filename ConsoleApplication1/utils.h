#pragma once
#include <string>
#include <array>
#include "types.h"
#include <string>

// Convert hexadecimal string to byte array
static void bytesFromHex(hash256_t& out, const std::string &hex);

// Convert byte array to hexadecimal string
void hexFromBytes(std::string& out, const hash256_t& bytes, const uint64_t& size);

// SHA-256 Hashing
void sha256Of(hash256_t& out, const void* data, const uint64_t& len);

// Little-endian uint64_t to byte array
std::array<uint8_t, 8> putUint64Le(const uint64_t& value);

// Sterilise transaction for hashing
void hashTransaction(hash256_t& out, const Transaction& tx);

// Sterilise UTXO for storage
std::array<uint8_t, 80> steriliseUtxo(UTXOKey key, UTXO Utxo);

// Sterilise block for storage
std::array<uint8_t, 96> steriliseBlockHeader(BlockHeader blockHeader);



