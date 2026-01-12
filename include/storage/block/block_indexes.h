#pragma once
#include <rocksdb/db.h>
#include "crypto_utils.h"

struct BlockIndexValue {
    uint64_t height{};
    Array256_t chainWork{};
};

rocksdb::DB* blockIndexesDb();

void putBlockIndexBatch(
    const std::vector<Array256_t>& hashes,
    const std::vector<BlockIndexValue>& values);

// Delete block index
void batchDeleteBlockIndex(

    const std::vector<Array256_t>& hashes);

// Get block index
std::optional<BlockIndexValue>
tryGetBlockIndex(const Array256_t& hash);