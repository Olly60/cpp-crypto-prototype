#include "storage/file_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_utils.h"
#include "../../../include/tip_block.h"

namespace
{
    // Hash function for UTXO keys
    struct UtxoKeyHash
    {
        std::size_t operator()(const std::pair<Array256_t, uint64_t>& key) const
        {
            // Hash the transaction hash (first 8 bytes) and output index
            std::size_t h1 = 0;
            std::memcpy(&h1, key.first.data(), std::min(sizeof(h1), uint64_t{key.first.size()}));
            const std::size_t h2 = std::hash<uint64_t>{}(key.second);
            return h1 ^ (h2 << 1);
        }
    };

    using UtxoSet = std::unordered_set<std::pair<Array256_t, uint64_t>, UtxoKeyHash>;

    // Calculate transaction fee (1% of input amount, minimum 1)
    constexpr uint64_t calculateTxFee(const uint64_t inputAmount)
    {
        return std::max(inputAmount / 100, static_cast<uint64_t>(1));
    }

    uint64_t getBlockReward(const BlockHeader& blockHeader)
    {
        //TODO: make function
    }
}

bool verifyNewMempoolTx(const Tx& tx)
{
    // Verify signatures
    if (!verifyTxSignature(tx))
    {
        return false;
    }

    // Open UTXO database
    const auto utxoDb = openDb(paths::utxosDb);

    // Track seen UTXOs to prevent double-spending within transaction
    UtxoSet seenUtxos;

    uint64_t totalInputAmount = 0;
    uint64_t totalOutputAmount = 0;

    // Verify inputs
    for (const TxInput& input : tx.txInputs)
    {
        // Check UTXO exists in database
        if (!utxoInDb(*utxoDb, input))
        {
            return false; // UTXO not found
        }

        // Check for duplicate inputs within this transaction
        if (auto utxoKey = std::make_pair(input.UTXOTxHash, input.UTXOOutputIndex); !seenUtxos.insert(utxoKey).second)
        {
            return false; // Double-spend attempt within transaction
        }

        // Accumulate input amount
        totalInputAmount += getUtxo(*utxoDb, input).amount;
    }

    // Verify outputs
    for (const TxOutput& output : tx.txOutputs)
    {
        // Check for zero or negative amounts (though uint64_t prevents negative)
        if (output.amount == 0)
        {
            return false; // Zero-value output not allowed
        }

        // Accumulate output amount
        totalOutputAmount += output.amount;

        // Check for overflow
        if (totalOutputAmount < output.amount)
        {
            return false; // Overflow detected
        }
    }

    // Verify that outputs don't exceed inputs (accounting for fee)
    if (uint64_t txFee = calculateTxFee(totalInputAmount); totalOutputAmount > totalInputAmount - txFee)
    {
        return false; // Output exceeds input minus fee
    }

    return true;
}

// ============================================================================
// FULL BLOCK VALIDATION
// ============================================================================

bool VerifyTmpHeaderchain(std::vector<BlockHeader>& headers)
{
    for (auto header : headers)
    {
        // Verify block header
        const auto prevHeader = getBlockHeader(getTipHash());
        const auto prevTimestamp2 = getBlockHeader(prevHeader.prevBlockHash).timestamp;
        const Array256_t blockHash = getBlockHeaderHash(header);

        // Version check
        if (header.version != 1) return false;

        // Previous block hash check
        if (header.prevBlockHash != getBlockHeaderHash(prevHeader)) return false;

        // Timestamp validations
        if (header.timestamp <= prevHeader.timestamp) return false;
        if (header.timestamp > getCurrentTimestamp() + (60 * 10)) return false;

        // Difficulty validation
        const uint64_t timeDelta = prevHeader.timestamp - prevTimestamp2;
        if (timeDelta < 60 * 10)
        {
            if (header.difficulty != increaseDifficulty(prevHeader.difficulty)) return false;
        }
        else
        {
            if (header.difficulty != decreaseDifficulty(prevHeader.difficulty)) return false;
        }

        // Hash meets difficulty
        if (header.merkleRoot != getMerkleRoot(txs)) return false;

    }
}

bool verifyTmpBlockchain(Array256_t block)
{

    // merkle root validation
    if (!isLessLE(blockHash, block.header.difficulty)) return false;



    // Verify transactions
    if (block.txs.empty()) return false;

    auto utxoDb = openDb(paths::utxosDb);
    UtxoSet blockUtxos;
    uint64_t totalFees = 0;

    // Verify non-coinbase transactions
    for (uint64_t i = 1; i < block.txs.size(); i++)
    {
        const Tx& tx = block.txs[i];

        if (!verifyNewMempoolTx(tx)) return false;

        uint64_t inputAmount = 0;
        for (const TxInput& input : tx.txInputs)
        {
            if (!blockUtxos.insert({input.UTXOTxHash, input.UTXOOutputIndex}).second) return false;
            inputAmount += getUtxo(*utxoDb, input).amount;
        }

        totalFees += calculateTxFee(inputAmount);
    }

    // Verify coinbase transaction
    const Tx& coinbaseTx = block.txs[0];
    const uint64_t coinbaseReward = getBlockReward(block.header) + totalFees;

    if (!coinbaseTx.txInputs.empty()) return false;
    if (coinbaseTx.txOutputs.empty()) return false;

    uint64_t coinbaseAmount = 0;
    for (const TxOutput& output : coinbaseTx.txOutputs)
    {
        coinbaseAmount += output.amount;
    }

    if (coinbaseAmount != coinbaseReward) return false;

    return true; // Block valid
}