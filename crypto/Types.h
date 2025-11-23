#pragma once
#include <array>
#include <vector>
using array256_t = std::array<uint8_t, 32>;

struct UTXO {
	uint64_t amount{0};
	array256_t recipient{};
};

struct TxInput {
	array256_t UTXOTxHash{};
	uint32_t UTXOOutputIndex{0};
	array256_t signature{};
};

struct Tx {
	uint8_t version{1};
	std::vector<TxInput> txInputs;
	std::vector<UTXO> txOutputs;
};

struct Block {
	uint8_t version{1};
	array256_t prevBlockHash{};
	array256_t merkleRoot{};
	uint64_t timestamp{};
	array256_t difficulty{};
	array256_t nonce{};
	std::vector<Tx> txs;
};