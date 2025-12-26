#include <filesystem>
#include "crypto_utils.h"
#include "storage/storage_utils.h"
#include "../include/tip.h"
#include <sodium/crypto_sign.h>

#include "verify.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

const std::filesystem::path TIP = "blockchain_tip";
const std::filesystem::path BLOCKS_PATH = "blocks";
const std::filesystem::path UNDO_PATH = "undo";

std::filesystem::path getBlockFilePath(const Array256_t& blockHash)
{
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    return BLOCKS_PATH / (bytesToHex(hashBuf) + ".block");
}

std::filesystem::path getUndoFilePath(const Array256_t& blockHash)
{
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    return UNDO_PATH / (bytesToHex(hashBuf) + ".undo");
}

Array256_t getTipHash()
{
    auto hash = readFile(TIP);
    return hash->readArray256();
}

uint64_t getTipHeight()
{
    auto blockIndexesDb = openBlockIndexesDb();
    return getBlockIndex(*blockIndexesDb, getTipHash()).height;
}

Array256_t getTipChainWork()
{
    auto blockIndexesDb = openBlockIndexesDb();
    return getBlockIndex(*blockIndexesDb, getTipHash()).chainWork;

}

Block getTipBlock()
{
    return *getBlock(getTipHash());
}

BlockHeader getTipHeader()
{
    return *getBlockHeader(getTipHash());
}

//----------------------------------------
// Add and undo blocks
//----------------------------------------
void addNewTipBlock(const Block& block)
{
    Array256_t blockHash = getBlockHeaderHash(block.header);
    std::filesystem::path blockFilePath = getBlockFilePath(blockHash);
    std::filesystem::path undoFilePath = getUndoFilePath(blockHash);

    auto utxoDb = openUtxoDb();

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

            TxOutput output = *tryGetUtxo(*utxoDb, input);

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
    undoFile.write(undoData.cdata(), undoData.size());

    // Write block file
    auto blockFile = openFileTruncWrite(blockFilePath);
    auto blockBytes = serialiseBlock(block);
    blockFile.write(blockBytes.cdata(), blockBytes.size());

    // Apply UTXO batch
    applyUtxoBatch(*utxoDb, spends, adds);

    // Update block height and chain work
    auto heightsDb = openHeightsDb();

    uint64_t blockHeight = getBlockIndex(*heightsDb, getTipHash()).height + 1;
    auto blockWork = getBlockWork(block.header.difficulty);

    putHeightHashBatch(*heightsDb, {blockHash});

    auto blockIndexesDb = openBlockIndexesDb();
    BlockIndexValue blockIndex;
    blockIndex.chainWork = addBlockWorkLe(getTipChainWork(), blockWork);
    blockIndex.height = blockHeight;
    putBlockIndex(*blockIndexesDb, blockHash, blockIndex);

    // Open tip file
    auto file = openFileTruncWrite(TIP);

    // Write new tip hash
    file.write(reinterpret_cast<const char*>(blockHash.data()), blockHash.size());
}

void undoNewTipBlock()
{
    // Block file path
    Array256_t blockHash = getTipHash();
    auto blockFilePath = getBlockFilePath(blockHash);

    // Undo file path
    auto undoFilePath = getUndoFilePath(blockHash);

    Block block = parseBlock(readFile(blockFilePath));
    auto utxoDb = openUtxoDb();

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
        auto undoDataBytes = readFile(undoFilePath);
        uint64_t txCount = undoDataBytes->readU64();

        for (uint64_t i = 0; i < txCount; i++)
        {
            undoDataBytes->readU64(); // version
            uint64_t inputCount = undoDataBytes->readU64();

            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                input.UTXOTxHash = undoDataBytes->readArray256();
                input.UTXOOutputIndex = undoDataBytes->readU64();

                TxOutput utxo;
                utxo.amount = undoDataBytes->readU64();
                utxo.recipient = undoDataBytes->readArray256();

                restores.emplace_back(input, utxo);
            }
        }
    }

    // Apply batch UTXO changes
    applyUtxoBatch(*utxoDb, spends, restores);

    // Remove block and undo files
    std::filesystem::remove(blockFilePath);
    std::filesystem::remove(undoFilePath);

    // Remove block from heights DB
    auto heightsDb = openHeightsDb();
    deleteHeightHashBatch(*heightsDb, 1);

    // Remove block from indexes DB
    auto blockIndexesDb = openBlockIndexesDb();
    deleteBlockIndex(*blockIndexesDb, blockHash);

    // Open tip file
    auto file = openFileTruncWrite(TIP);

    // Write new tip hash
    file.write(reinterpret_cast<const char*>(block.header.prevBlockHash.data()), block.header.prevBlockHash.size());
}


