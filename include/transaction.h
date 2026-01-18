#pragma once
#include <optional>
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
    bool operator==(const UTXOId&) const = default;
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
    Array256_t nonce{};
};

struct UTXOIdHash
{
    std::size_t operator()(const UTXOId& id) const noexcept
    {
        std::size_t h = 0;

        // Hash the 256-bit tx hash
        for (uint8_t b : id.UTXOTxHash)
        {
            h = h * 131 ^ b;
        }

        // Mix in output index
        h ^= std::hash<uint64_t>{}(id.UTXOOutputIndex)
             + 0x9e3779b97f4a7c15ULL
             + (h << 6)
             + (h >> 2);

        return h;
    }
};


Array256_t getTxHash(const Tx& tx);

Array256_t getMerkleRoot(const std::vector<Tx>& txs);


Array256_t computeTxSignHash(const Tx& tx, uint64_t inputIndex);

Tx signTxInputs(const Tx& tx, const Array512_t& sk);


BytesBuffer serialiseTx(const Tx& tx);

Tx parseTx(BytesBuffer& txBytes);


Tx makeTx(const std::unordered_set<UTXOId, UTXOIdHash>& utxos, const Array512_t& sk, const Array256_t& recipient, uint64_t amount);

