#pragma once
#include <array>
#include <vector>
#include <string>
typedef std::array<uint8_t, 32> array256_t;

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

struct TxData {
	std::vector<uint8_t> data;
	uint32_t TxInputs = 0;
	uint32_t TxOutputs = 0;
};

struct BlockData {
	std::vector<uint8_t> data;
	uint32_t Transactions;
};