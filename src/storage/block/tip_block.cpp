#include <filesystem>
#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "storage/block/tip_block.h"

#include "verify.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_indexes.h"

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

    std::unordered_set<std::pair<Array256_t, uint64_t>, UtxoKeyHash>;

    uint64_t getBlockReward(const BlockHeader& blockHeader)
    {
        //TODO: make function
    }
}

bool verifyNewTipBlock(const Block& block)
{
    // ----------------------------------------
    // Verify block header
    // ----------------------------------------
    const auto prevHeader = getBlockHeader(getTipHash());
    const auto prevTimestamp2 = getBlockHeader(prevHeader.prevBlockHash).timestamp;
    const Array256_t blockHash = getBlockHeaderHash(block.header);

    // Version check
    if (block.header.version != 1) return false;

    // Previous block hash check
    if (block.header.prevBlockHash != getBlockHeaderHash(prevHeader)) return false;

    // Timestamp validations
    if (block.header.timestamp <= prevHeader.timestamp) return false;
    if (block.header.timestamp > getCurrentTimestamp() + (60 * 10)) return false;

    // Difficulty validation
    const uint64_t timeDelta = prevHeader.timestamp - prevTimestamp2;
    if (timeDelta < 60 * 10)
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

    // ----------------------------------------
    // Verify transactions
    // ----------------------------------------

    // Check standard Utxo db
    auto utxoDb = openDb(paths::utxosDb);
    std::unordered_set<std::pair<Array256_t, uint64_t>, UtxoKeyHash> blockUtxos;
    uint64_t totalFees = 0;

    // Verify non-coinbase transactions
    for (uint64_t i = 1; i < block.txs.size(); ++i)
    {
        const Tx& tx = block.txs[i];

        // Verify signatures
        if (!verifyTxSignature(tx))
        {
            return false;
        }

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
            if (auto utxoKey = std::make_pair(input.UTXOTxHash, input.UTXOOutputIndex); !blockUtxos.insert(utxoKey).
                second)
            {
                return false; // Double-spend attempt within block
            }

            // Accumulate input amount
            totalInputAmount += getUtxo(*utxoDb, input).amount;
        }

        // Verify outputs
        for (const TxOutput& output : tx.txOutputs)
        {
            // Check for zero
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
        uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1));

        if (totalOutputAmount > totalInputAmount - txFee)
        {
            return false; // Output exceeds input minus fee
        }

        totalFees += txFee;
    }

    // ----------------------------------------
    // Verify coinbase transaction
    // ----------------------------------------

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


