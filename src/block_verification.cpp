#include <sodium.h>
#include "crypto_utils.h"
#include <stdexcept>
#include "block_verification.h"
#include "storage.h"
#include <set>
#include <unordered_set>

// ============================================================================
// VALIDATION HELPERS
// ============================================================================

namespace {
    // Hash function for UTXO keys
    struct UtxoKeyHash {
        std::size_t operator()(const std::pair<Array256_t, uint32_t>& key) const {
            // Hash the transaction hash (first 8 bytes) and output index
            std::size_t h1 = 0;
            std::memcpy(&h1, key.first.data(), std::min(sizeof(h1), key.first.size()));
            std::size_t h2 = std::hash<uint32_t>{}(key.second);
            return h1 ^ (h2 << 1);
        }
    };

    using UtxoSet = std::unordered_set<std::pair<Array256_t, uint32_t>, UtxoKeyHash>;

    // Calculate transaction fee (1% of input amount, minimum 1)
    constexpr uint64_t calculateTxFee(uint64_t inputAmount) {
        return std::max(inputAmount / 100, uint64_t(1));
    }

    // Maximum time drift allowed (10 minutes in seconds)
    constexpr uint64_t MAX_TIME_DRIFT = 60 * 10;
    
    uint64_t getBlockReward(const BlockHeader& blockHeader ) { 

}

}

// ============================================================================
// TRANSACTION VALIDATION
// ============================================================================

bool verifyTxSignature(const Tx& tx)
{
    auto utxoDb = openUtxoDb(); // open the UTXO database

    for (size_t i = 0; i < tx.txInputs.size(); i++) {
        const TxInput& in = tx.txInputs[i];

        // Check that the UTXO exists
        if (!utxoInDb(*utxoDb, in)) {
            return false; // trying to spend a non-existent UTXO
        }

        // Get the UTXO (previous output)
        TxOutput utxo = getUtxo(*utxoDb, in);

        // Compute the sighash for this input
        Array256_t hash = computeTxInputHash(tx, i);

        // Verify the signature against the public key stored in the UTXO
        if (crypto_sign_verify_detached(
            in.signature.data(),
            hash.data(),
            hash.size(),
            utxo.recipient.data() // public key of the UTXO owner
        ) != 0)
        {
            return false; // invalid signature
        }
    }

    return true; // all inputs are valid
}

bool verifyTx(const Tx& tx) {
    // Verify signatures
    if (!verifyTxSignature(tx)) {
        return false;
    }

    // Open UTXO database
    auto utxoDb = openUtxoDb();

    // Track seen UTXOs to prevent double-spending within transaction
    UtxoSet seenUtxos;

    uint64_t totalInputAmount = 0;
    uint64_t totalOutputAmount = 0;

    // Verify inputs
    for (const TxInput& input : tx.txInputs) {
        // Check UTXO exists in database
        if (!utxoInDb(*utxoDb, input)) {
            return false; // UTXO not found
        }

        // Check for duplicate inputs within this transaction
        auto utxoKey = std::make_pair(input.UTXOTxHash, input.UTXOOutputIndex);
        if (!seenUtxos.insert(utxoKey).second) {
            return false; // Double-spend attempt within transaction
        }

        // Accumulate input amount
        totalInputAmount += getUtxo(*utxoDb, input).amount;
    }

    // Verify outputs
    for (const TxOutput& output : tx.txOutputs) {
        // Check for zero or negative amounts (though uint64_t prevents negative)
        if (output.amount == 0) {
            return false; // Zero-value output not allowed
        }

        // Accumulate output amount
        totalOutputAmount += output.amount;

        // Check for overflow
        if (totalOutputAmount < output.amount) {
            return false; // Overflow detected
        }
    }

    // Verify that outputs don't exceed inputs (accounting for fee)
    uint64_t txFee = calculateTxFee(totalInputAmount);
    if (totalOutputAmount > totalInputAmount - txFee) {
        return false; // Output exceeds input minus fee
    }

    return true;
}

// ============================================================================
// BLOCK HEADER VALIDATION
// ============================================================================

namespace {
    bool verifyBlockHeader(const BlockHeader& header, const Array256_t& blockHash) {
        // Check version
        if (header.version != 1) {
            return false;
        }

        // Check if block already exists
        if (blockExists(blockHash)) {
            return false; // Block already in chain
        }

        // Check if previous block exists
        if (!blockExists(header.prevBlockHash)) {
            return false; // Previous block not found
        }

        // Get previous block header
        BlockHeader prevHeader = getBlockHeaderByHash(header.prevBlockHash);

        // Verify timestamp is not earlier than previous block
        if (header.timestamp <= prevHeader.timestamp) {
            return false; // Timestamp not increasing
        }

        // Verify timestamp is not too far in the future
        uint64_t currentTime = getCurrentTimestamp();
        if (header.timestamp > currentTime + MAX_TIME_DRIFT) {
            return false; // Timestamp too far in future
        }

        // TODO: Verify difficulty target
        


        // Verify proof-of-work (hash meets difficulty requirement)
        if (blockHash > header.difficulty) return false;

        return true;
    }
}

// ============================================================================
// COINBASE VALIDATION
// ============================================================================

namespace {
    bool verifyCoinbase(const Tx& coinbaseTx, uint64_t expectedReward) {
        // Coinbase must have no inputs
        if (!coinbaseTx.txInputs.empty()) {
            return false;
        }

        // Coinbase must have at least one output
        if (coinbaseTx.txOutputs.empty()) {
            return false;
        }

        // Calculate total coinbase amount
        uint64_t coinbaseAmount = 0;
        for (const TxOutput& output : coinbaseTx.txOutputs) {
            coinbaseAmount += output.amount;

            // Check for overflow
            if (coinbaseAmount < output.amount) {
                return false;
            }
        }

        // Coinbase amount must equal block reward + fees
        if (coinbaseAmount != expectedReward) {
            return false; // Coinbase amount incorrect
        }

        return true;
    }
}

// ============================================================================
// FULL BLOCK VALIDATION
// ============================================================================

bool verifyBlock(const Block& block) {
    // Check block has at least coinbase transaction
    if (block.txs.empty()) {
        return false;
    }

    // Calculate block hash
    const Array256_t blockHash = getBlockHash(block);

    // Verify merkle root matches transactions
    if (block.header.merkleRoot != getMerkleRoot(block.txs)) {
        return false;
    }

    // Verify block header
    if (!verifyBlockHeader(block.header, blockHash)) {
        return false;
    }

    // Open UTXO database once for all transactions
    auto utxoDb = openUtxoDb();

    // Track UTXOs used in this block to prevent double-spending
    UtxoSet blockUtxos;

    // Calculate total fees from all transactions
    uint64_t totalFees = 0;

    // Verify all non-coinbase transactions
    for (size_t i = 1; i < block.txs.size(); i++) {
        const Tx& tx = block.txs[i];

        // Verify transaction is valid
        if (!verifyTx(tx)) {
            return false;
        }

        uint64_t inputAmount = 0;
        uint64_t outputAmount = 0;

        // Check inputs and track UTXOs
        for (const TxInput& input : tx.txInputs) {
            // Check UTXO hasn't been used in this block already
            auto utxoKey = std::make_pair(input.UTXOTxHash, input.UTXOOutputIndex);
            if (!blockUtxos.insert(utxoKey).second) {
                return false; // Double-spend within block
            }

            // Accumulate input amount
            inputAmount += getUtxo(*utxoDb, input).amount;
        }

        // Calculate output amount
        for (const TxOutput& output : tx.txOutputs) {
            outputAmount += output.amount;
        }

        // Calculate and accumulate fee
        uint64_t txFee = calculateTxFee(inputAmount);
        totalFees += txFee;

        // Verify fee calculation is consistent with verifyTx
        if (outputAmount > inputAmount - txFee) {
            return false;
        }
    }
    uint64_t coinbaseReward = BLOCK_REWARD + totalFees;

    // Verify coinbase transaction (first transaction)
    if (!verifyCoinbase(block.txs[0], coinbaseReward)) {
        return false;
    }

    return true;
}

// ============================================================================
// SIMPLE VALIDATION HELPERS
// ============================================================================

bool verifyMerkleRoot(const Block& block) {
    return block.header.merkleRoot == getMerkleRoot(block.txs);
}

bool verifyBlockHash(const Block& block, const Array256_t& expectedHash) {
    return getBlockHash(block) == expectedHash;
}