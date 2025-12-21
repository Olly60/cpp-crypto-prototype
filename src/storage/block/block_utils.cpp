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
        auto undoFile = openFileTruncWrite(undoFilePath);

        BytesBuffer undoData;

        // Write transaction amount
        undoData.writeU64(block.txs.size());

        for (const auto& tx : block.txs)
        {

            // Write transaction version
            undoData.writeU64(tx.version);

            // Write input count
            undoData.writeU64(tx.txInputs.size());

            // For each input, write UTXO reference and value
            for (const auto& input : tx.txInputs)
            {
                undoData.writeU64(input.UTXOTxHash.size());
                undoData.writeU64(input.UTXOOutputIndex);

                // Retrieve and write UTXO value
                const TxOutput usedUtxo = getUtxo(utxoDb, input);
                undoData.writeU64(usedUtxo.amount);
                undoData.writeU64(usedUtxo.recipient.size());
            }
        }
        undoFile.write(undoData.cdata(), undoData.ssize());
    }

    void restoreFromUndoFile(const fs::path& undoFilePath, rocksdb::DB& utxoDb)
    {
        auto undoDataBytes = readWholeFile(undoFilePath);

        // Read transaction amount
        uint64_t txAmount = undoDataBytes.readU64();
        for (uint64_t i = 0; i < txAmount; i++)
        {
            // Read transaction version
            uint64_t version = undoDataBytes.readU64();

            // Read input count
            uint64_t inputCount = undoDataBytes.readU64();

            // Read each input and restore UTXO
            for (uint64_t j = 0; j < inputCount; j++)
            {
                TxInput input;
                input.UTXOTxHash = undoDataBytes.readArray256();
                input.UTXOOutputIndex = undoDataBytes.readU64();

                TxOutput utxo;
                utxo.amount = undoDataBytes.readU64();
                utxo.recipient = undoDataBytes.readArray256();

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

BytesBuffer readBlockFileBytes(const Array256_t& blockHash)
{
    return readWholeFile(getBlockFilePath(blockHash));
}

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash)
{
    const auto path = getBlockFilePath(blockHash);
    if (!fs::exists(path))
        throw std::runtime_error("File does not exist: " + path.string());

    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    constexpr auto headerSize = calculateBlockHeaderSize();
    BytesBuffer header(headerSize);

    try
    {
        file.read(header.cdata(), headerSize);
    }
    catch (const std::ios_base::failure& e)
    {
        throw std::runtime_error("Failed to read file " + path.string() + ": " + e.what());
    }

    return header;
}

void addBlock(const Block& block)
{
    // Determine file paths
    const Array256_t blockHash = getBlockHash(block.header);
    const fs::path blockFilePath = getBlockFilePath(blockHash);
    const fs::path undoFilePath = getUndoFilePath(blockHash);

    // Open UTXO database
    auto utxoDb = openDb(paths::utxosDb);

    // Write undo data before modifying UTXO set
    writeUndoFile(undoFilePath, block, *utxoDb);

    // Open block file for writing
    auto blockFile = openFileTruncWrite(blockFilePath);

    // Serialize block
    const auto blockBytes = serialiseBlock(block);

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
    const auto block = getBlock(tip);

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

BlockHeader getBlockHeader(const Array256_t& blockHash)
{
    auto headerBytes = readBlockFileHeaderBytes(blockHash);
    return parseBlockHeader(headerBytes);
}

Block getBlock(const Array256_t& blockHash)
{
    auto blockBytes = readBlockFileBytes(blockHash);
    return parseBlock(blockBytes);
}
