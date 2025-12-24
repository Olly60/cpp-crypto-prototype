#include "storage/block/block_utils.h"
#include "parse_serialise.h"
#include "blockchain.h"
#include "storage/file_utils.h"

std::optional<Block> getBlock(const Array256_t& blockHash)
{
    auto block = readFile(getBlockFilePath(blockHash));
    if (!block) return std::nullopt;
    return block;
}

std::optional<BlockHeader> getBlockHeader()
{

}

std::optional<BytesBuffer> getBlockBytes()
{

}

std::optional<BytesBuffer> getBlockHeaderBytes()
{

}
