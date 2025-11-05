#pragma once
#include <array>
#include <vector>
typedef std::array<uint8_t, 32> array256_t;

struct UTXO {
	array256_t txHash = {};
	uint8_t outputIndex = 0;
	uint64_t amount = 0;
	array256_t recipient = {};
};

struct TxInput {
	array256_t UTXOTxHash = {};
	uint8_t UTXOOutputIndex = 0;
	array256_t signature = {};
};

struct Transaction {
	std::array<TxInput, 64> txInputs;
	std::array<UTXO, 64> txOutputs;
};

struct Block {
	uint64_t version = 1;
	array256_t previousBlockHash = {};
	array256_t merkleRoot = {};
	uint64_t timestamp;
	array256_t difficulty = {};
	array256_t nonce = {};
	std::array<Transaction, 128> transactions;
};