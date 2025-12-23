#include "storage/file_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"
#include "storage/block/tip_block.h"
void addBlock(const Block& block)
{
    // Determine file paths
    Array256_t blockHash = getBlockHeaderHash(block.header);
    fs::path blockFilePath = getBlockFilePath(blockHash);
    fs::path undoFilePath = getUndoFilePath(blockHash);

    uint64_t blockHeight = getBlockIndex(*openDb(paths::blockIndexesDb), getTipHash()).height + 1;
    auto blockWork = getBlockWork(block.header.difficulty);

    // Open UTXO database
    auto utxoDb = openDb(paths::utxosDb);

    // Write undo data before modifying UTXO set
    writeUndoFile(undoFilePath, block, *utxoDb);

    // Open block file for writing
    auto blockFile = openFileTruncWrite(blockFilePath);

    // Serialize block
    auto blockBytes = serialiseBlock(block);

    // Write block data
    blockFile.write(blockBytes.cdata(), blockBytes.ssize());

    // Update UTXO set: add new UTXOs
    for (const auto& tx : block.txs)
    {
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t outputIndex = 0; outputIndex < tx.txOutputs.size(); outputIndex++)
        {
            TxInput utxoKey{txHash, outputIndex, {}};
            putUtxo(*utxoDb, utxoKey, tx.txOutputs[outputIndex]);
        }
    }

    // Update UTXO set: remove spent UTXOs
    for (const auto& tx : block.txs)
    {
        for (const auto& input : tx.txInputs)
        {
            deleteUtxo(*utxoDb, input);
        }
    }

    // Add block to heights db
    auto heightsDb = openDb(paths::blockHeightsDb);

    putHeightHash(*heightsDb, getBlockIndex(*openDb(paths::blockIndexesDb), blockHash).height, blockHash);

    // Write a block Index
    auto blockIndexesDb = openDb(paths::blockIndexesDb);

    BlockIndexValue blockIndex;
    blockIndex.chainWork = addBlockWork(getTipChainWork(), blockWork);
    blockIndex.height = blockHeight;
    putBlockIndex(*blockIndexesDb, blockHash, blockIndex);

    // Update blockchain tip
    setBlockchainTip(blockHash);
}

void undoBlock()
{
    const auto blockHash = getTipHash();
    const fs::path blockFilePath = getBlockFilePath(blockHash);
    const fs::path undoFilePath = getUndoFilePath(blockHash);

    // Read block
    auto block = getBlock(blockHash);

    // Open UTXO database
    const auto utxoDb = openDb(paths::utxosDb);

    // Remove created UTXOs
    for (const auto& tx : block.txs)
    {
        const Array256_t txHash = getTxHash(tx);
        for (uint64_t i = 0; i < tx.txOutputs.size(); i++)
        {
            TxInput utxoKey{txHash, i, {}};
            deleteUtxo(*utxoDb, utxoKey);
        }
    }

    // Restore spent UTXOs from undo file
    restoreFromUndoFile(undoFilePath, *utxoDb);

    // Update blockchain tip
    setBlockchainTip(block.header.prevBlockHash);

    // Delete files
    fs::remove(blockFilePath);
    fs::remove(undoFilePath);

    // Delete block from height db
    auto heightsDb = openDb(paths::blockHeightsDb);
    deleteHeightHash(*heightsDb, getBlockIndex(*openDb(paths::blockIndexesDb), blockHash).height);

    // Delete block from block indexes db
    auto blockIndexesDb = openDb(paths::blockIndexesDb);
    deleteBlockIndex(*blockIndexesDb, blockHash);
}