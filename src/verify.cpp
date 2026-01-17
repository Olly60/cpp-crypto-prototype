#include "verify.h"
#include <sodium/crypto_sign.h>
#include "tip.h"
#include "crypto_utils.h"
#include "storage/utxo_storage.h"
#include "block.h"
#include "block_work.h"
#include "storage/block/block_indexes.h"

bool verifyTx(const Tx& tx, VerifyTxContext ctx)
{
    uint64_t totalInputAmount = 0;
    uint64_t totalOutputAmount = 0;

    // Create a local fallback
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq> localSeenUtxo;

    // Determine which one to use once
    auto* activeSeenUtxos = ctx.seenUtxos ? ctx.seenUtxos : &localSeenUtxo;

    for (uint64_t i = 0; i < tx.txInputs.size(); ++i)
    {
        auto utxoInDb = tryGetUtxo(tx.txInputs[i]);

        if (ctx.includeUtxos) // if included is defined then check it for the utxo as well
        {
            // Utxo not in database and not in the included list
            if (!utxoInDb && ctx.includeUtxos->erase(tx.txInputs[i]) == 0) return false;
        }
        else
        {
            // Utxo not in database
            if (!utxoInDb) return false;
        }


        Array256_t hash = computeTxSignHash(tx, i);

        // Verify signature
        if (crypto_sign_verify_detached(
            tx.txInputs[i].signature.data(),
            hash.data(),
            hash.size(),
            utxoInDb->recipient.data()) != 0)
        {
            return false;
        }


        // Double spend
        if (ctx.seenUtxos)
        {
            if (!activeSeenUtxos->insert(tx.txInputs[i]).second) // Check its not in seen (double spend)
                return false;


            totalInputAmount += utxoInDb->amount;
        }
    }

    for (const auto& output : tx.txOutputs)
    {
        if (output.amount == 0) return false;
        totalOutputAmount += output.amount;
    }

    // validate amount is enough to cover fee
    uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1));
    if (totalOutputAmount > totalInputAmount - txFee) return false;

    if (ctx.totalFees) *ctx.totalFees += txFee;

    return true;
}

bool verifyBlockHeader(const BlockHeader& header, VerifyBlockHeaderContext ctx)
{
    // Resolve defaults
    const BlockHeader prevHeader =
        ctx.prevHeader ? *ctx.prevHeader : *getBlockHeader(getTipHash());

    const uint64_t prevPrevTimestamp = ctx.prevPrevTimestamp ? *ctx.prevPrevTimestamp : tryGetBlockIndex(getTipHash())->height > 0 ? getBlockHeader(getBlockHeader(getTipHash())->prevBlockHash)->timestamp : 0;

    Array256_t blockHash = getBlockHeaderHash(header);

    // Version check
    if (header.version != 1)
        return false;

    // Previous block hash check
    if (header.prevBlockHash != getBlockHeaderHash(prevHeader))
        return false;

    // Timestamp checks
    if (header.timestamp <= prevHeader.timestamp)
        return false;

    if (header.timestamp > getCurrentTimestamp() + 600)
        return false;

    // Difficulty adjustment (target-based)
    uint64_t timeDelta =
    (prevHeader.timestamp > prevPrevTimestamp)
        ? prevHeader.timestamp - prevPrevTimestamp
        : 0;

    Array256_t expectedDifficulty =
        (timeDelta < 600)
            ? shiftRight(prevHeader.difficulty)
            : shiftLeft(prevHeader.difficulty);


    if (header.difficulty != expectedDifficulty)
        return false;

    // Proof-of-work: hash must be <= target
    if (blockHash > header.difficulty) return false;

    return true;
}

bool verifyBlock(const ChainBlock& block, VerifyBlockContext ctx)
{
    // ---------------------------
    // Verify block header
    // ---------------------------
    if (!verifyBlockHeader(block.header, ctx.headerCtx))
        return false;

    // Merkle root check
    if (block.header.merkleRoot != getMerkleRoot(block.txs))
        return false;

    // ---------------------------
    // Verify transactions
    // ---------------------------
    uint64_t totalFees = 0;

    // Ensure tx verification can accumulate fees
    ctx.txCtx.totalFees = &totalFees;

    for (size_t i = 1; i < block.txs.size(); ++i) // skip coinbase
    {
        if (!verifyTx(block.txs[i], ctx.txCtx))
            return false;
    }

    // ---------------------------
    // Verify coinbase transaction
    // ---------------------------
    const Tx& coinbaseTx = block.txs[0];

    if (!coinbaseTx.txInputs.empty())
        return false;

    if (coinbaseTx.txOutputs.empty())
        return false;


    uint64_t expectedReward = 5000000000 + totalFees;

    uint64_t coinbaseAmount = 0;
    for (const TxOutput& output : coinbaseTx.txOutputs)
    {
        coinbaseAmount += output.amount;
    }

    if (coinbaseAmount != expectedReward)
        return false;

    return true;
}
