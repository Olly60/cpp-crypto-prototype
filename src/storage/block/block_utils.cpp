#include "storage/block/block_utils.h"
#include "storage/file_utils.h"
#include "crypto_utils.h"
#include <filesystem>
#include <fstream>
#include <rocksdb/db.h>
#include "storage/utxo_storage.h"
#include "../../../include/tip_block.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"

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

//----------------------------------------
// Block reading
//----------------------------------------
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

    constexpr uint64_t headerSize = calculateBlockHeaderSize();
    BytesBuffer header(headerSize);

    file.read(header.cdata(), headerSize);
    return header;
}

bool blockExists(const Array256_t& blockHash)
{
    return fs::exists(getBlockFilePath(blockHash));
}

BlockHeader getBlockHeader(const Array256_t& blockHash)
{
    return parseBlockHeader(readBlockFileHeaderBytes(blockHash));
}

Block getBlock(const Array256_t& blockHash)
{
    return parseBlock(readBlockFileBytes(blockHash));
}

