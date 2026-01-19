#pragma once
#include <rocksdb/db.h>
#include "crypto_utils.h"

struct BlockIndexValue
{
    uint64_t height{};
    Array256_t chainWork{};
};

rocksdb::DB* blockIndexesDb();

void putBlockIndexBatch(const std::vector<std::pair<Array256_t, BlockIndexValue>>& indexes);

void batchDeleteBlockIndex(const std::vector<Array256_t>& hashes);

std::optional<BlockIndexValue> tryGetBlockIndex(const Array256_t& hash);
