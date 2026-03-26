#include <filesystem>
#include <iostream>

#include "tip.h"
#include "crypto_utils.h"
#include "storage/storage_utils.h"
#include "block_work.h"
#include "node.h"
#include "verify.h"
#include "wallet.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "block.h"

Array256_t getTipHash()
{
    auto hash = readFile(TIP);
    return hash->readArray256();
}

void addNewTipBlock(const ChainBlock& block)
{
    Array256_t blockHash = getBlockHeaderHash(block.header);
    std::filesystem::path blockFilePath = getBlockFilePath(blockHash);
    std::filesystem::path undoFilePath = getUndoFilePath(blockHash);

    BytesBuffer undoData;

    // Prepare UTXO batch
    std::vector<std::pair<UTXOId, TxOutput>> adds;
    std::vector<UTXOId> spends;

    undoData.writeU64(block.txs.size());

    for (const auto& tx : block.txs)
    {
        undoData.writeU64(tx.version);
        undoData.writeU64(tx.txInputs.size());

        // Process inputs
        for (const auto& input : tx.txInputs)
        {
            TxOutput output = *tryGetUtxo(input.utxoId);

            undoData.writeArray256(input.utxoId.UTXOTxHash);
            undoData.writeU64(input.utxoId.UTXOOutputIndex);
            undoData.writeU64(output.amount);
            undoData.writeArray256(output.recipient);

            spends.push_back(input.utxoId);

            if (wallets.contains(output.recipient))
            {
                wallets[output.recipient].erase(input.utxoId);
            }
        }

        // Process outputs
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
        {
            adds.push_back({{txHash, i}, tx.txOutputs[i]});

            if (wallets.contains(tx.txOutputs[i].recipient))
            {
                wallets[tx.txOutputs[i].recipient].insert({txHash, i});
            }
        }
    }

    // Write undo file
    writeFileTrunc(undoFilePath, undoData.data(), undoData.size());

    // Write block file
    auto blockBytes = serialiseBlock(block);
    writeFileTrunc(blockFilePath, blockBytes.data(), blockBytes.size());

    // Apply UTXO batch
    applyUtxoBatch(spends, adds);

    // Update block height and chain work
    putHeightHashBatch({blockHash});

    BlockIndexValue blockIndex;
    blockIndex.chainWork = addBlockWork(tryGetBlockIndex(getTipHash())->chainWork,
                                        getBlockWork(block.header.difficulty));
    blockIndex.height = tryGetBlockIndex(getTipHash())->height + 1;
    putBlockIndexBatch({{blockHash, blockIndex}});

    // Write new tip hash
    writeFileTrunc(TIP, blockHash.data(), blockHash.size());

    // Remove any used transactions from the mempool
    for (auto& tx : block.txs)
    {
        mempool.erase(getTxHash(tx));
    }
}

void undoNewTipBlock()
{
    Array256_t blockHash = getTipHash();

    ChainBlock block = *getBlock(blockHash);

    // Collect UTXOs created by this block for deletion
    std::vector<UTXOId> spends;
    for (const auto& tx : block.txs)
    {
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
        {
            spends.push_back({txHash, i});
            if (wallets.contains(tx.txOutputs[i].recipient))
            {
                wallets[tx.txOutputs[i].recipient].erase({txHash, i});
            }
        }
    }

    // Undo file path
    auto undoFilePath = getUndoFilePath(blockHash);

    // Restore spent UTXOs from undo file
    std::vector<std::pair<UTXOId, TxOutput>> restores;
    {
        auto undoDataBytes = readFile(undoFilePath);
        uint64_t txCount = undoDataBytes->readU64();

        for (uint64_t i = 0; i < txCount; i++)
        {
            // Version
            undoDataBytes->readU64();

            uint64_t inputCount = undoDataBytes->readU64();

            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                input.utxoId.UTXOTxHash = undoDataBytes->readArray256();
                input.utxoId.UTXOOutputIndex = undoDataBytes->readU64();

                TxOutput utxo;
                utxo.amount = undoDataBytes->readU64();
                utxo.recipient = undoDataBytes->readArray256();

                restores.emplace_back(input.utxoId, utxo);

                if (wallets.contains(utxo.recipient))
                {
                    wallets[utxo.recipient].insert(input.utxoId);
                }
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
    writeFileTrunc(TIP, block.header.prevBlockHash.data(), block.header.prevBlockHash.size());
}


