#pragma once
#include <rocksdb/db.h>
#include "crypto_utils.h"

struct BlockIndexValue {
    uint64_t height{};
    Array256_t chainWork{};
};

void putBlockIndex(rocksdb::DB& db, const Array256_t& hash, const std::pair<uint64_t, Array256_t>& value);


void deleteBlockIndex(rocksdb::DB& db, const Array256_t& hash);


BlockIndexValue getBlockIndex(rocksdb::DB& db, const Array256_t& hash);