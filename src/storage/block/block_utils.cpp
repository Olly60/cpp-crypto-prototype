#include "storage/block/block_utils.h"
#include "parse_serialise.h"
#include "blockchain.h"
#include "storage/file_utils.h"

std::optional<Block> getBlock(const Array256_t& blockHash)
{
    auto block = parseBlock(readFile(getBlockFilePath(blockHash)));
    if (!block) return std::nullopt;
    return block;
}

std::optional<BlockHeader> getBlockHeader(const Array256_t& blockHash)
{
    auto BlockHeader = parseBlockHeader(readFile(getBlockFilePath(blockHash), calculateBlockHeaderSize())):
    if (!BlockHeader) return std::nullopt;
    return BlockHeader;
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
