#include "block.h"
#include <asio/ip/address.hpp>
#include "tip.h"
#include "transaction.h"
#include "storage/storage_utils.h"


std::filesystem::path getBlockFilePath(const Array256_t& blockHash)
{
    return std::filesystem::path("blocks") / (bytesToHex(blockHash.data(), blockHash.size()) + ".dat");
}

std::filesystem::path getUndoFilePath(const Array256_t& blockHash)
{
    return std::filesystem::path("undo") / (bytesToHex(blockHash.data(), blockHash.size()) +  ".dat");
};

std::optional<ChainBlock> getBlock(const Array256_t& blockHash)
{
    auto blockBytes = readFile(getBlockFilePath(blockHash));
    if (!blockBytes) return std::nullopt;
    auto block = parseBlock(*blockBytes);
    return block;
}

std::optional<BlockHeader> getBlockHeader(const Array256_t& blockHash)
{
    auto headerBytes = readFile(getBlockFilePath(blockHash));
    if (!headerBytes) return std::nullopt;
    auto blockHeader = parseBlockHeader(*headerBytes);
    return blockHeader;
}

std::optional<BytesBuffer> getBlockBytes(const Array256_t& blockHash)
{
    auto blockBytes = readFile(getBlockFilePath(blockHash));
    if (!blockBytes) return std::nullopt;
    return blockBytes;
}

std::optional<BytesBuffer> getBlockHeaderBytes(const Array256_t& blockHash)
{
    auto blockHeaderBytes = readFile(getBlockFilePath(blockHash), calculateBlockHeaderSize());
    if (!blockHeaderBytes) return std::nullopt;
    return blockHeaderBytes;
}

Array256_t getBlockHeaderHash(const BlockHeader& header)
{
    return sha256Of(serialiseBlockHeader(header));
}

// ----------------------------------------
// BlockHeader
// ----------------------------------------
BytesBuffer serialiseBlockHeader(const BlockHeader& header)
{
    BytesBuffer headerBytes;
    headerBytes.writeU64(header.version);
    headerBytes.writeArray256(header.prevBlockHash);
    headerBytes.writeArray256(header.merkleRoot);
    headerBytes.writeU64(header.timestamp);
    headerBytes.writeArray256(header.difficulty);
    headerBytes.writeArray256(header.nonce);
    return headerBytes;
}

BlockHeader parseBlockHeader(BytesBuffer& headerBytes)
{
    BlockHeader header;
    header.version = headerBytes.readU64();
    header.prevBlockHash = headerBytes.readArray256();
    header.merkleRoot = headerBytes.readArray256();
    header.timestamp = headerBytes.readU64();
    header.difficulty = headerBytes.readArray256();
    header.nonce = headerBytes.readArray256();
    return header;
}

// ----------------------------------------
// Block
// ----------------------------------------
BytesBuffer serialiseBlock(const ChainBlock& block)
{

    BytesBuffer blockBytes;

    // Header
    blockBytes.writeBytesBuffer(serialiseBlockHeader(block.header));

    // Transaction amount
    blockBytes.writeU64(block.txs.size());

    // Transactions
    for (const auto& tx : block.txs)
    {
        blockBytes.writeBytesBuffer(serialiseTx(tx));
    }

    return blockBytes;
}

ChainBlock parseBlock(BytesBuffer& blockBytes)
{
    ChainBlock block;

    // Header
    block.header = parseBlockHeader(blockBytes);

    // Transaction amount
    uint64_t txCount = blockBytes.readU64();
    block.txs.reserve(txCount);

    // Transactions
    for (uint64_t i = 0; i < txCount; i++)
    {
        block.txs.push_back(parseTx(blockBytes));
    }

    return block;
}
