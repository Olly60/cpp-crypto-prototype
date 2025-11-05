#pragma once
#include <array>
#include <vector>
using hash256_t = std::array<uint8_t, 32>;

struct UTXO {
	hash256_t txHash;
	uint8_t outputIndex;
	uint64_t amount;
	hash256_t recipient;
};

struct TxInput {
	hash256_t UTXOTxHash;
	uint8_t UTXOOutputIndex;
	hash256_t signature;
};

struct Transaction {
	std::array<TxInput, 256> txInputs;
	std::array<UTXO, 256> txOutputs;
};

struct Block {
	uint64_t version;
	hash256_t previousBlockHash;
	hash256_t merkleRoot;
	uint64_t timestamp;
	uint64_t difficulty;
	uint64_t nonce;
	std::array<Transaction, 256> transactions;
};