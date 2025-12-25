#pragma once
#include <rocksdb/db.h>
#include "crypto_utils.h"

struct BlockIndexValue {
    uint64_t height{};
    Array256_t chainWork{};
    BlockIndexValue() = default;
};

std::unique_ptr<rocksdb::DB> openBlockIndexesDb();

void putBlockIndexBatch(
    rocksdb::DB& db,
    const std::vector<Array256_t>& hashes,
    const std::vector<BlockIndexValue>& values);

// Delete block index
void batchDeleteBlockIndex(
    rocksdb::DB& db,
    const std::vector<Array256_t>& hashes);

// Get block index
std::optional<BlockIndexValue>
tryGetBlockIndex(rocksdb::DB& db, const Array256_t& hash);