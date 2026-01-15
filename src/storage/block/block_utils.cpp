#include "storage/block/block_utils.h"

#include <asio/ip/address.hpp>

#include "parse_serialise.h"
#include "tip.h"
#include "storage/storage_utils.h"

std::string bytesToHex(const BytesBuffer& bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);

    for (const auto& byte : bytes)
    {
        constexpr char hexChars[] = "0123456789ABCDEF";
        hex.push_back(hexChars[byte >> 4]);
        hex.push_back(hexChars[byte & 0x0F]);
    }

    return hex;
};

std::filesystem::path getBlockFilePath(const Array256_t& blockHash)
{
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    return std::filesystem::path("blocks") / bytesToHex(hashBuf);
}

std::filesystem::path getUndoFilePath(const Array256_t& blockHash)
{
    BytesBuffer hashBuf;
    hashBuf.writeArray256(blockHash);
    return std::filesystem::path("undo") / bytesToHex(hashBuf);
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
