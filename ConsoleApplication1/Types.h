#pragma once
#include <array>
#include <vector>
#include <string>
typedef std::array<uint8_t, 32> array256_t;
#define inputSize 65
#define outputSize 40
#define inputAmountSize 4
#define outputAmountSize 4
#define txAmountSize 4

struct UTXO {
	uint64_t amount = 0;
	array256_t recipient = {};
};

struct TxInput {
	array256_t UTXOTxHash = {};
	uint32_t UTXOOutputIndex = 0;
	array256_t signature = {};
};

struct Transaction {
	std::vector<TxInput> txInputs;
	std::vector<UTXO> txOutputs;
};

struct Block {
	uint64_t version = 1;
	array256_t previousBlockHash = {};
	array256_t merkleRoot = {};
	uint64_t timestamp;
	array256_t difficulty = {};
	array256_t nonce = {};
	std::vector<Transaction> transactions;
};