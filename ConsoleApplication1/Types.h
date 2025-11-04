#pragma once
#include <array>
#include <vector>
#include <cstdint>
using hash256_t = std::array<uint8_t, 32>;

struct UTXO {
    uint64_t amount;
    hash256_t recipient;
};

struct TxInput {
    hash256_t prevTxHash;
    uint64_t outputIndex;
};

struct TxInputSigned {
    TxInput txInput;
    hash256_t signature;
};

struct Transaction {
    std::vector<TxInputSigned> txInputs;
    std::vector<UTXO> txOutputs;
};

struct BlockHeader {
    uint64_t version;
    hash256_t previousBlockHash;
    hash256_t merkleRoot;
    uint64_t timestamp;
    uint64_t difficulty;
    uint64_t nonce;
};

struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;
};

struct UTXOKey {
    hash256_t txHash;
    uint64_t outputIndex;
};

struct UTXOKeyHash {
    size_t operator()(const UTXOKey& key) const noexcept;
};
