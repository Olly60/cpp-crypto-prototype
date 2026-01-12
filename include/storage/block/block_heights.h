#pragma once
#include "crypto_utils.h"
#include <rocksdb/db.h>

rocksdb::DB* heightsDb();

std::optional<Array256_t> tryGetHeightHash(
    uint64_t height);

void putHeightHashBatch(

    const std::vector<Array256_t>& hashes);

void deleteHeightHashBatch(
    uint64_t amount);
