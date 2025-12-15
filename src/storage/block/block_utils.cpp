#include "storage/block/block_utils.h"
#include "storage/file_utils.h"
#include "crypto_utils.h"
#include <filesystem>
#include <fstream>
#include <rocksdb/db.h>
#include "storage/utxo_storage.h"
#include "storage/block/tip_block.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"

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
            appendToFile(undoFile, tx.txInputs.size());

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
            parseBytesInto(version, undoDataBytes, offset);

            // Read input count
            uint64_t inputCount;
            parseBytesInto(inputCount, undoDataBytes, offset);

            // Read each input and restore UTXO
            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                parseBytesInto(input.UTXOTxHash, undoDataBytes, offset);
                parseBytesInto(input.UTXOOutputIndex, undoDataBytes, offset);

                TxOutput utxo;
                parseBytesInto(utxo.amount, undoDataBytes, offset);
                parseBytesInto(utxo.recipient, undoDataBytes, offset);

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
}

std::vector<uint8_t> readBlockFileBytes(const Array256_t& blockHash)
{
    return readWholeFile(getBlockFilePath(blockHash));
}

std::vector<uint8_t> readBlockFileHeaderBytes(const Array256_t& blockHash)
{
    const auto path = getBlockFilePath(blockHash);
    if (!fs::exists(path))
        throw std::runtime_error("File does not exist: " + path.string());

    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    constexpr auto headerSize = calculateBlockHeaderSize();
    std::vector<uint8_t> header(headerSize);

    try
    {
        file.read(reinterpret_cast<char*>(header.data()),
                  static_cast<std::streamsize>(headerSize));
    }
    catch (const std::ios_base::failure& e)
    {
        throw std::runtime_error("Failed to read file " + path.string() + ": " + e.what());
    }

    return header;
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

    putHeightHash(*heightsDb, getBlockIndex(*openDb(paths::blockIndexesDb), getTipHash()).height, blockHash);
}

void undoBlock()
{
    const auto tip = getTipHash();
    const fs::path blockFilePath = getBlockFilePath(tip);
    const fs::path undoFilePath = getUndoFilePath(tip);

    // Read block
    const auto block = getBlockByHash(tip);

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
    const auto heightsDb = openDb(paths::blockHeightsDb);
    deleteHeightHash(*heightsDb, getBlockIndex(*openDb(paths::blockIndexesDb), getTipHash()).height);
}

bool blockExists(const Array256_t& blockHash)
{
    return fs::exists(getBlockFilePath(blockHash));
}

BlockHeader getBlockHeaderByHash(const Array256_t& blockHash)
{
    return parseBlockHeader(readBlockFileHeaderBytes(blockHash));
}

Block getBlockByHash(const Array256_t& blockHash)
{
    return parseBlock(readBlockFileBytes(blockHash));
}
