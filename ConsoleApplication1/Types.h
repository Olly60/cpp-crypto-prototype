#pragma once
#include <array>
#include <vector>
using hash256_t = std::array<uint8_t, 32>;

// Unspent Transaction Output Key
struct UTXOKey {
	hash256_t txHash;
	uint64_t outputIndex;
};
// Unspent Transaction Output
struct UTXO {
	uint64_t amount;
	hash256_t recipient;
};

// Transaction Input Decides which UTXO to Spend
struct TxInput {
	hash256_t prevTxHash;
	uint64_t outputIndex;
};

// Signed Transaction Input
struct TxInputSigned {
	TxInput txInput;
	hash256_t signature;
};

// Transaction
struct Transaction {
	std::vector<TxInputSigned> txInputs;
	std::vector<UTXO> txOutputs;
};

// Block Header
struct BlockHeader {
	uint64_t version;
	hash256_t previousBlockHash;
	hash256_t merkleRoot;
	uint64_t timestamp;
	uint64_t difficulty;
	uint64_t nonce;

};

// Block
struct Block {
	BlockHeader header;
	std::vector<Transaction> transactions;
};