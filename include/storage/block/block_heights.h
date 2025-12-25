#pragma once
#include "crypto_utils.h"
#include <rocksdb/db.h>

std::unique_ptr<rocksdb::DB> openHeightsDb();

std::optional<Array256_t> tryGetHeightHash(rocksdb::DB& db,
    uint64_t height);

void putHeightHashBatch(
    rocksdb::DB& db,
    const std::vector<Array256_t>& hashes);

void deleteHeightHashBatch(  rocksdb::DB& db,
    uint64_t amount);
