#pragma once
#include <filesystem>
#include <optional>
#include "crypto_utils.h"
#include "transaction.h"

struct BlockHeader
{
    uint64_t version = 1;
    Array256_t prevBlockHash{};
    Array256_t merkleRoot{};
    uint64_t timestamp = 0;
    Array256_t difficulty{};
    Array256_t nonce{};

    BlockHeader()
    {
        prevBlockHash.fill(0xFF);
        // Starting difficulty
        difficulty.fill(0xFF);
        difficulty[0] = 0x00;
        difficulty[1] = 0x00;
    }
};

struct ChainBlock
{
    BlockHeader header;
    std::vector<Tx> txs{};
};

std::filesystem::path getBlockFilePath(const Array256_t& blockHash);

std::filesystem::path getUndoFilePath(const Array256_t& blockHash);

std::optional<ChainBlock> getBlock(const Array256_t& blockHash);

std::optional<BlockHeader> getBlockHeader(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockBytes(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockHeaderBytes(const Array256_t& blockHash);

Array256_t getBlockHeaderHash(const BlockHeader& header);

constexpr size_t calculateBlockHeaderSize()
{
    return sizeof(decltype(BlockHeader::version)) // version
        + sizeof(decltype(BlockHeader::prevBlockHash)) // prevBlockHash
        + sizeof(decltype(BlockHeader::merkleRoot)) // merkleRoot
        + sizeof(decltype(BlockHeader::timestamp)) // timestamp
        + sizeof(decltype(BlockHeader::difficulty)) // difficulty
        + sizeof(decltype(BlockHeader::nonce)); // nonce
}

BytesBuffer serialiseBlockHeader(const BlockHeader& header);

BlockHeader parseBlockHeader(BytesBuffer& headerBytes);

BytesBuffer serialiseBlock(const ChainBlock& block);

ChainBlock parseBlock(BytesBuffer& blockBytes);