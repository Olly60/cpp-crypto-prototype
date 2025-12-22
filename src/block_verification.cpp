#include <sodium.h>
#include "crypto_utils.h"
#include <stdexcept>
#include "block_verification.h"
#include <unordered_set>
#include "storage/file_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/tip_block.h"

// ============================================================================
// VALIDATION HELPERS
// ============================================================================

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

    // Maximum time drift allowed (10 minutes in seconds)
    constexpr uint64_t MAX_TIME_DRIFT = 60 * 10;

    // Block interval (10 minutes)
    constexpr uint64_t BLOCK_INTERVAL = 60 * 10;

    uint64_t getBlockReward(const BlockHeader& blockHeader)
    {
        //TODO: make function
    }
}

// ============================================================================
// TRANSACTION VALIDATION
// ============================================================================

namespace
{
    bool verifyTxSignature(const Tx& tx)
    {
        // Public key must come from the input / referenced output
        auto utxoDb = openDb(paths::utxosDb);

        for (auto& txInput : tx.txInputs)
        {
            // Recompute the hash that was signed
            Array256_t hash = computeTxInputHash(tx);

            if (crypto_sign_verify_detached(
                    txInput.signature.data(),
                    hash.data(),
                    hash.size(),
                    getUtxo(*utxoDb, txInput).recipient.data()) != 0)
            {
                return false; // invalid signature
            }
        }

        return true;
    }
}

bool verifyTx(const Tx& tx)
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
// BLOCK HEADER VALIDATION
// ============================================================================

bool verifyBlockHeader(const BlockHeader& header, const BlockHeader& prevHeader, const uint64_t prevTimestamp2)
    {

        Array256_t blockHash = getBlockHeaderHash(header);

        // Check version
        if (header.version != 1)
        {
            return false;
        }

        // Check previous header matches merkle root
        if (header.prevBlockHash != getBlockHeaderHash(prevHeader)) {return false; }

        // Verify timestamp is not earlier than previous block
        if (header.timestamp <= prevHeader.timestamp)
        {
            return false; // Timestamp not increasing
        }

        // Verify timestamp is not too far in the future
        if (header.timestamp > getCurrentTimestamp() + MAX_TIME_DRIFT)
        {
            return false; // Timestamp too far in future
        }

        // Difficulty too small
        Array256_t minDifficulty;
        minDifficulty.fill(0xFF);
        if (header.difficulty > minDifficulty) return false;

        // Difficulty target
        if (prevHeader.timestamp - prevTimestamp2 < BLOCK_INTERVAL)
        {
            if (header.difficulty != increaseDifficulty(prevHeader.difficulty)) {return false;}

        } else if (prevHeader.timestamp - prevTimestamp2 >= 10 * 60)
        {
            if (header.difficulty != decreaseDifficulty(prevHeader.difficulty)) {return false;}
        }

        // Hash meets difficulty requirement
        if (!isLessLE(blockHash, header.difficulty)) return false;

        return true;
    }

// ============================================================================
// COINBASE VALIDATION
// ============================================================================

namespace
{
    bool verifyCoinbase(const Tx& coinbaseTx, const uint64_t expectedReward)
    {
        // Coinbase must have no inputs
        if (!coinbaseTx.txInputs.empty())
        {
            return false;
        }

        // Coinbase must have at least one output
        if (coinbaseTx.txOutputs.empty())
        {
            return false;
        }

        // Calculate total coinbase amount
        uint64_t coinbaseAmount = 0;
        for (const TxOutput& output : coinbaseTx.txOutputs)
        {
            coinbaseAmount += output.amount;

        }

        // Coinbase amount must equal block reward + fees
        if (coinbaseAmount != expectedReward)
        {
            return false; // Coinbase amount incorrect
        }

        return true;
    }
}

// ============================================================================
// FULL BLOCK VALIDATION
// ============================================================================

bool verifyBlock(const Block& block, const BlockHeader& prevHeader, const uint64_t prevTimestamp2)
{
    // Verify block header
    if (!verifyBlockHeader(block.header, prevHeader, prevTimestamp2))
    {
        return false;
    }

    // Check block has at least coinbase transaction
    if (block.txs.empty())
    {
        return false;
    }

    // Verify merkle root matches transactions
    if (block.header.merkleRoot != getMerkleRoot(block.txs))
    {
        return false;
    }

    // Open UTXO database once for all transactions
    const auto utxoDb = openDb(paths::utxosDb);

    // Track UTXOs used in this block to prevent double-spending
    UtxoSet blockUtxos;

    // Calculate total fees from all transactions
    uint64_t totalFees = 0;

    // Verify all non-coinbase transactions
    for (size_t i = 1; i < block.txs.size(); i++)
    {
        const Tx& tx = block.txs[i];

        // Verify transaction is valid
        if (!verifyTx(tx))
        {
            return false;
        }

        uint64_t inputAmount = 0;
        uint64_t outputAmount = 0;

        // Check inputs and track UTXOs
        for (const TxInput& input : tx.txInputs)
        {
            // Check UTXO hasn't been used in this block already
            if (!blockUtxos.insert(std::make_pair(input.UTXOTxHash, input.UTXOOutputIndex)).second)
            {
                return false; // Double-spend within block
            }

            // Accumulate input amount
            inputAmount += getUtxo(*utxoDb, input).amount;
        }

        // Calculate output amount
        for (const TxOutput& output : tx.txOutputs)
        {
            outputAmount += output.amount;
        }

        // Calculate and accumulate fee
        totalFees += calculateTxFee(inputAmount);
    }

    // Verify coinbase transaction (first transaction)
    if (const uint64_t coinbaseReward = getBlockReward(block.header) + totalFees; !verifyCoinbase(block.txs[0], coinbaseReward))
    {
        return false;
    }

    return true;
}