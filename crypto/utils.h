#pragma once
#include "utils.h"
#include "types.h"
#include <vector>
#include <array>
#include <string>

array256_t bytesFromHex(const std::string& hex);

std::string hexFromBytes(const array256_t& bytes, const uint64_t& len);

array256_t sha256Of(const uint8_t* data, const uint64_t& len);

// Format number to native from little endian
template <typename T>
T formatNumber(const uint8_t* in);

// Serialise number to little endian
template <typename T>
std::array<uint8_t, sizeof(T)> serialiseNumber(const T& in);

std::vector<uint8_t> serialiseTxInput(const TxInput& in, uint64_t version);

std::vector<uint8_t> serialiseUTXO(const UTXO& utxo, uint64_t version);

std::vector<uint8_t> serialiseTx(const Tx& tx, uint64_t version);

std::vector<uint8_t> serialiseBlock(const Block& block);

TxInput formatTxInput(const uint8_t* data, uint64_t version);

UTXO formatUTXO(const uint8_t* data, uint64_t version);

Tx formatTx(const uint8_t* data, uint64_t version);

Block formatBlock(const uint8_t* data);
