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
                auto [amount, recipient] = getUtxo(utxoDb, input);
                undoData.writeU64(amount);
                undoData.writeU64(recipient.size());
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
            // Read transaction version (for later use)
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
