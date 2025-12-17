#include <filesystem>
#include <stdexcept>
#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "storage/block/tip_block.h"

void setBlockchainTip(const Array256_t& newTip)
{
    fs::create_directories(paths::blockchainTip.parent_path());

    // Open file
    auto file = openFileTruncWrite(paths::blockchainTip);

    // Tip buffer
    BytesBuffer tipBuffer;
    tipBuffer << newTip;

    // Write new tip hash
    file.write(tipBuffer.cdata(), tipBuffer.ssize());
}

Array256_t getTipHash()
{
    Array256_t hash;
    readWholeFile(paths::blockchainTip) >> hash;
    return hash;
}


