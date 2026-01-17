#pragma once
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "crypto_utils.h"


struct TxOutput
{
    uint64_t amount = 0;
    Array256_t recipient{};
};

struct UTXOId
{
    Array256_t UTXOTxHash{};
    uint64_t UTXOOutputIndex = 0;
};

struct TxInput
{
    UTXOId utxoId;
    Array512_t signature{};
};

struct Tx
{
    uint64_t version = 1;
    std::vector<TxInput> txInputs{};
    std::vector<TxOutput> txOutputs{};
};


inline std::unordered_map<Array256_t, std::unordered_set<{    Array256_t UTXOTxHash{}; uint64_t UTXOOutputIndex; }>, Array256Hash> wallets;


Array256_t getTxHash(const Tx& tx);

Array256_t getMerkleRoot(const std::vector<Tx>& txs);



Array256_t computeTxSignHash(const Tx& tx, uint64_t inputIndex);

Tx signTxInputs(const Tx& tx, const Array512_t& sk);



BytesBuffer serialiseTx(const Tx& tx);

Tx parseTx(BytesBuffer& txBytes);

