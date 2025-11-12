#pragma once
#include "types.h"

// Convert hexadecimal string to byte array
static void bytesFromHex(array256_t& out, const std::string &hex);

// Convert byte array to hexadecimal string
void hexFromBytes(std::string& out, const array256_t& bytes, const uint64_t& size);

// SHA-256 Hashing
void sha256Of(array256_t& out, const void* data, const uint64_t& len);

// Little-endian uint64_t to byte array
std::array<uint8_t, 8> putUint64Le(const uint64_t& value);

// Serialise data
std::array<uint8_t, 65> serialiseTxInput(const TxInput& txInput);

std::array<uint8_t, 40> serialiseUTXO(const UTXO &Utxo);

std::array<uint8_t, 65> serialiseTxInput(const TxInput &txInput);

void serialiseTx(std::vector<uint8_t>& out, uint32_t& inputCount, uint32_t& outputCount, const Transaction &tx);

void serialiseBlock(std::vector<uint8_t>& out, uint32_t& txCount, const Block &block);

// Format data







