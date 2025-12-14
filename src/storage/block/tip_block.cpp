#include <filesystem>
#include <stdexcept>
#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "storage/block/tip_block.h"

void setBlockchainTip(const Array256_t& newTip)
{
    fs::create_directories(paths::blockchainTip.parent_path());

    // Get new height
    auto tipBytes = readWholeFile(paths::blockchainTip);
    uint64_t currentHeight;
    takeBytesInto(currentHeight, {tipBytes.data() + sizeof(Array256_t), sizeof(currentHeight)});

    // open file
    std::ofstream blockchainTipFile(paths::blockchainTip, std::ios::trunc | std::ios::binary);
    if (!blockchainTipFile)
    {
        throw std::runtime_error("Failed to open blockchain tip file for writing");
    }

    appendToFile(blockchainTipFile, newTip);


}

Array256_t getTipHash()
{
    auto fileBytes = readWholeFile(paths::blockchainTip);
    Array256_t hash;
    takeBytesInto(hash, fileBytes);
    return hash;
}


