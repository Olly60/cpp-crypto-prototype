#pragma once
#include <array>
#include <vector>
typedef std::array<uint8_t, 32> array256_t;

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
	std::vector<TxInput> txInputs;
	std::vector<UTXO> txOutputs;
};

struct Block {
	uint64_t version{1};
	array256_t prevBlockHash{};
	array256_t merkleRoot{};
	uint64_t timestamp{};
	array256_t difficulty{};
	array256_t nonce{};
	std::vector<Tx> transactions;
};