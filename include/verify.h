#pragma once
#include <unordered_set>
#include "crypto_utils.h"

// Hash function for UTXO keys
struct TxInputKeyHash
{
    std::size_t operator()(const TxInput& key) const
    {
        std::size_t h = 0;

        // Hash txid (32 bytes)
        for (size_t i = 0; i < 32; ++i)
        {
            h = (h * 131) ^ key.UTXOTxHash[i];
        }

        // Hash output index
        h ^= std::hash<uint64_t>{}(key.UTXOOutputIndex) + 0x9e3779b97f4a7c15ULL
            + (h << 6) + (h >> 2);

        return h;
    }
};

struct TxInputKeyEq
{
    bool operator()(const TxInput& a, const TxInput& b) const noexcept
    {
        return a.UTXOTxHash == b.UTXOTxHash &&
            a.UTXOOutputIndex == b.UTXOOutputIndex;
    }
};


struct VerifyBlockHeaderContext
{
    const BlockHeader* prevHeader = nullptr;
    const BlockHeader* prevPrevHeader = nullptr;
};


struct VerifyTxContext
{
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq>* seenUtxos = nullptr;
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq>* includeUtxos = nullptr;
    uint64_t* totalFees = nullptr;
};

struct VerifyBlockContext
{
    VerifyBlockHeaderContext& headerOptions;
    VerifyTxContext& txOptions;
};

bool verifyTx(const Tx& tx, VerifyTxContext& ctx);

bool verifyBlockHeader(const BlockHeader& header, VerifyBlockHeaderContext ctx);

bool verifyBlock(const Block& block, const VerifyBlockOptions& options);
