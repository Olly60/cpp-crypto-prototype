#include <filesystem>
#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "../include/tip_block.h"
#include <sodium/crypto_sign.h>
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

void setBlockchainTip(const Array256_t& newTip)
{
    fs::create_directories(paths::blockchainTip.parent_path());

    // Open file
    auto file = openFileTruncWrite(paths::blockchainTip);

    // Tip buffer
    BytesBuffer tipBuffer;
    tipBuffer.writeArray256(newTip);

    // Write new tip hash
    file.write(tipBuffer.cdata(), tipBuffer.ssize());
}

Array256_t getTipHash()
{
    auto hash = readWholeFile(paths::blockchainTip);
    return hash.readArray256();
}

uint64_t getTipHeight()
{
    auto blockIndexesDb = openDb(paths::blockIndexesDb);
    return getBlockIndex(*blockIndexesDb, getTipHash()).height;
}

Array256_t getTipChainWork()
{
    auto blockIndexesDb = openDb(paths::blockIndexesDb);
    return getBlockIndex(*blockIndexesDb, getTipHash()).chainWork;

}

namespace
{
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

    bool verifyTxSignature(const Tx& tx)
    {
        // Open UTXO DB once
        auto utxoDb = openDb(paths::utxosDb);

        for (const auto& txInput : tx.txInputs)
        {
            // Lookup the UTXO being spent
            TxOutput referencedOutput;
            if (!tryGetUtxo(*utxoDb, referencedOutput, txInput))
            {
                return false; // input references missing UTXO
            }

            // Recompute the hash that was signed for this input
            Array256_t hash = computeTxHash(tx);

            // Verify the signature using the public key from the referenced output
            if (crypto_sign_verify_detached(
                    txInput.signature.data(),
                    hash.data(),
                    hash.size(),
                    referencedOutput.recipient.data() // public key
                ) != 0)
            {
                return false; // invalid signature
            }
        }

        return true; // all signatures valid
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
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq> seenUtxos;

    uint64_t totalInputAmount = 0;
    uint64_t totalOutputAmount = 0;

    // Verify inputs
    for (const TxInput& input : tx.txInputs)
    {
        // Check UTXO exists in database
        TxOutput utxo;
        if (!tryGetUtxo(*utxoDb, utxo, input))
        {
            return false; // UTXO not found
        }

        // Check for duplicate inputs within this transaction
        if (!seenUtxos.insert(input).second)
        {
            return false; // Double-spend attempt within transaction
        }

        // Accumulate input amount
        totalInputAmount += utxo.amount;
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
    if (uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1)); totalOutputAmount >
        totalInputAmount - txFee)
    {
        return false; // Output exceeds input minus fee
    }

    return true;
}

bool verifyNewTipBlock(const Block& block)
    {
        // ---------------------------
        // Verify block header
        // ---------------------------
        const auto prevHeader = getBlockHeader(getTipHash());
        const auto prevPrevHeader = getBlockHeader(prevHeader.prevBlockHash);
        const Array256_t blockHash = getBlockHeaderHash(block.header);

        // Version check
        if (block.header.version != 1) return false;

        // Previous block hash check
        if (block.header.prevBlockHash != getBlockHeaderHash(prevHeader)) return false;

        // Timestamp validation
        if (block.header.timestamp <= prevHeader.timestamp) return false;
        if (block.header.timestamp > getCurrentTimestamp() + 600) return false; // 10 min max drift

        // Difficulty adjustment (simplified two-block check; could use window for stability)
        const uint64_t timeDelta = prevHeader.timestamp - prevPrevHeader.timestamp;
        if (timeDelta < 600)
        {
            if (block.header.difficulty != increaseDifficulty(prevHeader.difficulty)) return false;
        }
        else
        {
            if (block.header.difficulty != decreaseDifficulty(prevHeader.difficulty)) return false;
        }

        // Hash meets difficulty
        if (!isLessLE(blockHash, block.header.difficulty)) return false;

        // Merkle root validation
        if (block.header.merkleRoot != getMerkleRoot(block.txs)) return false;

        // ---------------------------
        // Verify transactions
        // ---------------------------
        auto utxoDb = openDb(paths::utxosDb);
        std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq> blockSpentUtxos;
        uint64_t totalFees = 0;

        for (size_t i = 1; i < block.txs.size(); ++i) // skip coinbase
        {
            const Tx& tx = block.txs[i];

            // Verify transaction signatures
            if (!verifyTxSignature(tx)) return false;

            uint64_t totalInputAmount = 0;
            uint64_t totalOutputAmount = 0;

            // Verify inputs
            for (const TxInput& input : tx.txInputs)
            {
                TxOutput utxo;
                if (!tryGetUtxo(*utxoDb, utxo, input)) return false;

                // Prevent double-spend within the block
                if (!blockSpentUtxos.insert(input).second) return false;

                totalInputAmount += utxo.amount;
            }

            // Verify outputs
            for (const TxOutput& output : tx.txOutputs)
            {
                if (output.amount == 0) return false; // zero output not allowed
                totalOutputAmount += output.amount;
            }

            // Transaction fee (integer-safe)
            uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1));
            if (totalOutputAmount > totalInputAmount - txFee) return false;

            totalFees += txFee;
        }

        // ---------------------------
        // Verify coinbase transaction
        // ---------------------------
        const Tx& coinbaseTx = block.txs[0];

        if (!coinbaseTx.txInputs.empty()) return false;
        if (coinbaseTx.txOutputs.empty()) return false;

        uint64_t expectedReward = 5000000000 / ((getTipHeight()+1) % 210000) + totalFees;

        uint64_t coinbaseAmount = 0;
        for (const TxOutput& output : coinbaseTx.txOutputs)
        {
            coinbaseAmount += output.amount;
        }

        if (coinbaseAmount != expectedReward) return false;

        return true; // block is valid
    }

//----------------------------------------
// Helper functions for undo files
//----------------------------------------

//----------------------------------------
// Block file paths
//----------------------------------------
namespace
{
    fs::path getBlockFilePath(const Array256_t& blockHash)
    {
        BytesBuffer hashBuf;
        hashBuf.writeArray256(blockHash);
        return paths::blocks / (bytesToHex(hashBuf) + ".block");
    }

    fs::path getUndoFilePath(const Array256_t& blockHash)
    {
        BytesBuffer hashBuf;
        hashBuf.writeArray256(blockHash);
        return paths::undo / (bytesToHex(hashBuf) + ".undo");
    }
}

//----------------------------------------
// Add and undo blocks
//----------------------------------------
void addNewTipBlock(const Block& block)
{
    Array256_t blockHash = getBlockHeaderHash(block.header);
    auto blockFilePath = getBlockFilePath(blockHash);
    auto undoFilePath = getUndoFilePath(blockHash);

    auto utxoDb = openDb(paths::utxosDb);

    // Open undo file
    auto undoFile = openFileTruncWrite(undoFilePath);
    BytesBuffer undoData;

    // Prepare UTXO batch
    std::vector<std::pair<TxInput, TxOutput>> adds;
    std::vector<TxInput> spends;

    undoData.writeU64(block.txs.size());

    for (const auto& tx : block.txs)
    {
        undoData.writeU64(tx.version);
        undoData.writeU64(tx.txInputs.size());

        // Process inputs (undo data + spends)
        for (const auto& input : tx.txInputs)
        {
            undoData.writeU64(input.UTXOTxHash.size());
            undoData.writeU64(input.UTXOOutputIndex);

            TxOutput output;
            tryGetUtxo(*utxoDb, output, input);

            undoData.writeU64(output.amount);
            undoData.writeU64(output.recipient.size());

            spends.push_back(input);
        }

        // Process outputs (adds)
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
        {
            adds.emplace_back(TxInput{txHash, i, {}}, tx.txOutputs[i]);
        }
    }

    // Write undo file
    undoFile.write(undoData.cdata(), undoData.ssize());

    // Write block file
    auto blockFile = openFileTruncWrite(blockFilePath);
    auto blockBytes = serialiseBlock(block);
    blockFile.write(blockBytes.cdata(), blockBytes.ssize());

    // Apply UTXO batch
    applyUtxoBatch(*utxoDb, spends, adds);

    // Update block height and chain work
    uint64_t blockHeight = getBlockIndex(*openDb(paths::blockIndexesDb), getTipHash()).height + 1;
    auto blockWork = getBlockWork(block.header.difficulty);

    auto heightsDb = openDb(paths::blockHeightsDb);
    putHeightHash(*heightsDb, blockHeight, blockHash);

    auto blockIndexesDb = openDb(paths::blockIndexesDb);
    BlockIndexValue blockIndex;
    blockIndex.chainWork = addBlockWork(getTipChainWork(), blockWork);
    blockIndex.height = blockHeight;
    putBlockIndex(*blockIndexesDb, blockHash, blockIndex);

    setBlockchainTip(blockHash);
}

void undoNewTipBlock()
{
    Array256_t blockHash = getTipHash();
    auto blockFilePath = getBlockFilePath(blockHash);
    auto undoFilePath = getUndoFilePath(blockHash);

    Block block = getBlock(blockHash);
    auto utxoDb = openDb(paths::utxosDb);

    // Collect UTXOs created by this block for deletion
    std::vector<TxInput> spends;
    for (const auto& tx : block.txs)
    {
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
            spends.push_back(TxInput{txHash, i, {}});
    }

    // Restore spent UTXOs from undo file
    std::vector<std::pair<TxInput, TxOutput>> restores;
    {
        auto undoDataBytes = readWholeFile(undoFilePath);
        uint64_t txCount = undoDataBytes.readU64();

        for (uint64_t i = 0; i < txCount; i++)
        {
            undoDataBytes.readU64(); // version
            uint64_t inputCount = undoDataBytes.readU64();

            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                input.UTXOTxHash = undoDataBytes.readArray256();
                input.UTXOOutputIndex = undoDataBytes.readU64();

                TxOutput utxo;
                utxo.amount = undoDataBytes.readU64();
                utxo.recipient = undoDataBytes.readArray256();

                restores.emplace_back(input, utxo);
            }
        }
    }

    // Apply batch UTXO changes
    applyUtxoBatch(*utxoDb, spends, restores);

    // Update blockchain tip
    setBlockchainTip(block.header.prevBlockHash);

    // Remove block and undo files
    fs::remove(blockFilePath);
    fs::remove(undoFilePath);

    // Remove block from heights DB
    auto heightsDb = openDb(paths::blockHeightsDb);
    deleteHeightHash(*heightsDb, getBlockIndex(*openDb(paths::blockIndexesDb), blockHash).height);

    // Remove block from indexes DB
    auto blockIndexesDb = openDb(paths::blockIndexesDb);
    deleteBlockIndex(*blockIndexesDb, blockHash);
}


