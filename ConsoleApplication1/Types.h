#pragma once
#include <array>
#include <vector>
using hash256_t = std::array<uint8_t, 32>;

// Custom hash function for UTXOKey
struct UTXOKeyHash {
	size_t operator()(const UTXOKey& key) const noexcept {
		const uint64_t* chunks = reinterpret_cast<const uint64_t*>(key.txHash.data());
		size_t data = chunks[0] ^ chunks[1] ^ chunks[2] ^ chunks[3];
		data ^= key.outputIndex + 0x9e3779b97f4a7c15ULL + (data << 6) + (data >> 2);
		return data;
	}
};

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
	vector<TxInputSigned> txInputs;
	vector<UTXO> txOutputs;
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
	vector<Transaction> transactions;
};