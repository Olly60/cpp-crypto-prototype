#include <filesystem>
#include <stdexcept>
#include "crypto_utils.h"
#include "storage/file_utils.h"

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

std::pair<Array256_t, uint64_t> getBlockchainTip()
{
    auto fileBytes = readWholeFile(paths::blockchainTip);

    Array256_t hash;
    uint64_t height;
    size_t offset = 0;
    takeBytesInto(hash, fileBytes, offset);
    takeBytesInto(height, fileBytes, offset);

    return {hash, height};
}

