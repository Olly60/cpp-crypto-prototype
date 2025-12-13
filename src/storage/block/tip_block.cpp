#include <filesystem>
#include <stdexcept>
#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "storage/block/tip_block.h"

void setBlockchainTip(const Array256_t& newTip, const bool undo)
{
    fs::create_directories(paths::blockchainTip.parent_path());

    // Get new height
    auto tipBytes = readWholeFile(paths::blockchainTip);
    uint64_t currentHeight;
    takeBytesInto(currentHeight, {tipBytes.data() + sizeof(Array256_t), sizeof(currentHeight)});
    uint64_t newHeight;
    if (undo)
    {
        newHeight = currentHeight - 1;

    } else
    {
        newHeight = currentHeight + 1;
    }


    // open file
    std::ofstream blockchainTipFile(paths::blockchainTip, std::ios::trunc | std::ios::binary);
    if (!blockchainTipFile)
    {
        throw std::runtime_error("Failed to open blockchain tip file for writing");
    }

    appendToFile(blockchainTipFile, newTip);
    appendToFile(blockchainTipFile, newHeight);


}

Array256_t getTipHash()
{
    auto fileBytes = readWholeFile(paths::blockchainTip);
    Array256_t hash;
    takeBytesInto(hash, fileBytes);
    return hash;
}

uint64_t getTipBlockHeight()
{
    auto fileBytes = readWholeFile(paths::blockchainTip);
    uint64_t currentHeight;
    size_t offset = sizeof(Array256_t);
    takeBytesInto(currentHeight, fileBytes, offset);
    return currentHeight;
}

Array512_t getTipBlockChainWork()
{
    auto fileBytes = readWholeFile(paths::blockchainTip);
    Array512_t totalWork;
    size_t offset = sizeof(Array256_t) + sizeof(uint64_t);
    takeBytesInto(totalWork, fileBytes, offset);
    return totalWork;
}


