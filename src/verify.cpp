#include "verify.h"
#include <sodium/crypto_sign.h>
#include "tip.h"
#include "crypto_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_utils.h"

bool verifyTx(const Tx& tx, VerifyTxContext ctx = {})
{
    auto utxoDb = openUtxoDb();
    uint64_t totalInputAmount = 0;
    uint64_t totalOutputAmount = 0;

    for (const auto& txInput : tx.txInputs)
    {
        auto utxo = tryGetUtxo(*utxoDb, txInput);

        if (ctx.includeUtxos)
        {
            if (!utxo || !ctx.includeUtxos->contains(txInput)) return false;
        } else
        {
            if (!utxo) return false;
        }


        Array256_t hash = computeTxHash(tx);

        if (crypto_sign_verify_detached(
                txInput.signature.data(),
                hash.data(),
                hash.size(),
                utxo->recipient.data()) != 0)
        {
            return false;
        }


        // Double spend
        if (ctx.seenUtxos)
        {
            if (!ctx.seenUtxos->insert(txInput).second)
                return false;
        }

        totalInputAmount += utxo->amount;
    }

    for (const auto& output : tx.txOutputs)
    {
        if (output.amount == 0) return false;
        totalOutputAmount += output.amount;
    }

    uint64_t txFee = std::max(totalInputAmount / 100, uint64_t(1));
    if (totalOutputAmount > totalInputAmount - txFee)
        return false;

    if (ctx.totalFees)
        *ctx.totalFees += txFee;

    return true;
}

bool verifyBlockHeader(const BlockHeader& header, VerifyBlockHeaderContext ctx)
{
    // Resolve defaults
    const BlockHeader& prevHeader =
        ctx.prevHeader ? *ctx.prevHeader : getTipHeader();

    const BlockHeader& prevPrevHeader =
        ctx.prevPrevHeader
            ? *ctx.prevPrevHeader
            : *getBlockHeader(prevHeader.prevBlockHash);

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
    uint64_t timeDelta = prevHeader.timestamp - prevPrevHeader.timestamp;

    Array256_t expectedDifficulty =
        (timeDelta < 600)
            ? increaseDifficulty(prevHeader.difficulty)
            : decreaseDifficulty(prevHeader.difficulty);

    if (header.difficulty != expectedDifficulty)
        return false;

    // Proof-of-work: hash must be <= target
    if (!isLessLE(blockHash, header.difficulty))
        return false;

    return true;
}


bool verifyBlock(const Block& block, const VerifyBlockOptions& options)
{
    // ---------------------------
    // Verify block header
    // ---------------------------
    if (!verifyBlockHeader(block.header, options)) return false;

    // Merkle root validation
    if (block.header.merkleRoot != getMerkleRoot(block.txs)) return false;

    // ---------------------------
    // Verify transactions
    // ---------------------------

    uint64_t totalFees = 0;

    for (size_t i = 1; i < block.txs.size(); ++i) // skip coinbase
    {
        verifyTx(block.txs[i], options.txOptions);
    }

    // ---------------------------
    // Verify coinbase transaction
    // ---------------------------
    const Tx& coinbaseTx = block.txs[0];

    if (!coinbaseTx.txInputs.empty()) return false;
    if (coinbaseTx.txOutputs.empty()) return false;

    uint64_t expectedReward = 5000000000 / ((getTipHeight() + 1) % 210000) + totalFees;

    uint64_t coinbaseAmount = 0;
    for (const TxOutput& output : coinbaseTx.txOutputs) coinbaseAmount += output.amount;

    if (coinbaseAmount != expectedReward) return false;

    return true; // block is valid
}
