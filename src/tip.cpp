#include <filesystem>
#include "crypto_utils.h"
#include "storage/storage_utils.h"
#include "../include/tip.h"
#include "verify.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

Array256_t getTipHash()
{
    auto hash = readFile(TIP);
    return hash->readArray256();
}

//----------------------------------------
// Add and undo blocks
//----------------------------------------
void addNewTipBlock(const ChainBlock& block)
{
    Array256_t blockHash = getBlockHeaderHash(block.header);
    std::filesystem::path blockFilePath = getBlockFilePath(blockHash);
    std::filesystem::path undoFilePath = getUndoFilePath(blockHash);

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

            TxOutput output = *tryGetUtxo(input);

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
    writeFileTrunc(undoFilePath, undoData);

    // Write block file
    auto blockBytes = serialiseBlock(block);
    writeFileTrunc(blockFilePath, blockBytes);

    // Apply UTXO batch
    applyUtxoBatch(spends, adds);

    // Update block height and chain work
    uint64_t blockHeight = tryGetBlockIndex(getTipHash())->height + 1;
    auto blockWork = getBlockWork(block.header.difficulty);

    putHeightHashBatch({blockHash});

    BlockIndexValue blockIndex;
    blockIndex.chainWork = addBlockWork(tryGetBlockIndex(getTipHash())->chainWork, blockWork);
    blockIndex.height = blockHeight;
    putBlockIndexBatch({blockHash}, {blockIndex});

    // Write new tip hash
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    writeFileTrunc(TIP, hashBuf);
}

void undoNewTipBlock()
{
    Array256_t blockHash = getTipHash();

    ChainBlock block = *getBlock(blockHash);

    // Collect UTXOs created by this block for deletion
    std::vector<TxInput> spends;
    for (const auto& tx : block.txs)
    {
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
            spends.push_back(TxInput{txHash, i, {}});
    }

    // Undo file path
    auto undoFilePath = getUndoFilePath(blockHash);

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
    applyUtxoBatch(spends, restores);

    // Remove block and undo files
    auto blockFilePath = getBlockFilePath(blockHash);
    std::filesystem::remove(blockFilePath);
    std::filesystem::remove(undoFilePath);

    // Remove block from heights DB
    deleteHeightHashBatch(1);

    // Remove block from indexes DB
    batchDeleteBlockIndex({blockHash});

    // Write new tip hash
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    writeFileTrunc(TIP, hashBuf);
}


