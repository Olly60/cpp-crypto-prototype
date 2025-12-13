#include "storage/block/block_utils.h"
#include "storage/file_utils.h"
#include "crypto_utils.h"
#include <filesystem>
#include <fstream>
#include <rocksdb/db.h>
#include "storage/utxo_storage.h"
#include "storage/block/tip_block.h"
#include "storage/block/block_heights.h"

// Block undo helpers
namespace
{

    void writeUndoFile(const fs::path& undoFilePath,
                       const Block& block,
                       rocksdb::DB& utxoDb)
    {
        auto undoFile = openFileForAppend(undoFilePath);

        for (const auto& tx : block.txs)
        {
            // Write transaction version
            appendToFile(undoFile, tx.version);

            // Write input count
            appendToFile(undoFile, static_cast<uint64_t>(tx.txInputs.size()));

            // For each input, write UTXO reference and value
            for (const auto& input : tx.txInputs)
            {
                appendToFile(undoFile, input.UTXOTxHash);
                appendToFile(undoFile, input.UTXOOutputIndex);

                // Retrieve and write UTXO value
                TxOutput usedUtxo = getUtxo(utxoDb, input);
                appendToFile(undoFile, usedUtxo.amount);
                appendToFile(undoFile, usedUtxo.recipient);
            }
        }
    }

    void restoreFromUndoFile(const fs::path& undoFilePath, rocksdb::DB& utxoDb)
    {
        auto undoDataBytes = readWholeFile(undoFilePath);
        size_t offset = 0;

        while (offset < undoDataBytes.size())
        {
            // Read transaction version
            uint64_t version;
            takeBytesInto(version, undoDataBytes, offset);

            // Read input count
            uint64_t inputCount;
            takeBytesInto(inputCount, undoDataBytes, offset);

            // Read each input and restore UTXO
            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                takeBytesInto(input.UTXOTxHash, undoDataBytes, offset);
                takeBytesInto(input.UTXOOutputIndex, undoDataBytes, offset);

                TxOutput utxo;
                takeBytesInto(utxo.amount, undoDataBytes, offset);
                takeBytesInto(utxo.recipient, undoDataBytes, offset);

                // Restore UTXO
                putUtxo(utxoDb, input, utxo);
            }
        }
    }
}

// Block file operations
namespace
{
    fs::path getBlockFilePath(const Array256_t& blockHash)
    {
        return paths::blocks / (bytesToHex(blockHash) + ".block");
    }

    fs::path getUndoFilePath(const Array256_t& blockHash)
    {
        return paths::undo / (bytesToHex(blockHash) + ".undo");
    }

    std::vector<uint8_t> readBlockFile(const Array256_t& blockHash)
    {
        return readWholeFile(getBlockFilePath(blockHash));
    }


    std::vector<uint8_t> readBlockFileHeader(const Array256_t& blockHash)
    {
        auto blockBytes = readBlockFile(blockHash);

        constexpr size_t headerSize = calculateBlockHeaderSize();
        if (blockBytes.size() < headerSize)
        {
            throw std::runtime_error("Block file too small to contain header");
        }

        return {blockBytes.begin(), blockBytes.begin() + headerSize};
    }
}

void addBlock(const Block& block)
{
    const Array256_t blockHash = getBlockHash(block);
    const fs::path blockFilePath = getBlockFilePath(blockHash);
    const fs::path undoFilePath = getUndoFilePath(blockHash);

    // Open UTXO database
    auto utxoDb = openDb(paths::utxosDb);

    // Write undo data before modifying UTXO set
    writeUndoFile(undoFilePath, block, *utxoDb);

    // Write block to disk
    auto blockFile = openFileForAppend(blockFilePath);
    const auto blockBytes = serialiseBlock(block);
    appendToFile(blockFile, blockBytes);

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

    // Update blockchain tip
    setBlockchainTip(blockHash);

    // Add block to heights db
    auto heightsDb = openDb(paths::blockHeightsDb);
    putBlockHeightHash()
}

void undoBlock()
{
    const auto tip = getBlockchainTip();
    const fs::path blockFilePath = getBlockFilePath(tip.first);
    const fs::path undoFilePath = getUndoFilePath(tip.first);

    // Read block
    const auto block = getBlockByHash(tip.first);

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
    setBlockchainTip(block.header.prevBlockHash, true);

    // Delete files
    fs::remove(blockFilePath);
    fs::remove(undoFilePath);

    // Delete block from height db
    deleteBlockHeightHash(getBlockchainTip().second);
}

bool blockExists(const Array256_t& blockHash)
{
    return fs::exists(getBlockFilePath(blockHash));
}

BlockHeader getBlockHeaderByHash(const Array256_t& blockHash)
{
    return parseBlockHeader(readBlockFileHeader(blockHash));
}

BlockHeader getBlockHeaderByHeight(const uint64_t& height)
{
    return parseBlockHeader(readBlockFileHeader(getBlockHeightHash(height)));
}

Block getBlockByHash(const Array256_t& blockHash)
{
    return parseBlock(readBlockFile(blockHash));
}
