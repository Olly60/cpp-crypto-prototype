#pragma once
#include "crypto_utils.h"
#include <rocksdb/db.h>
// Put block hash by height
void putHeightHash(rocksdb::DB& db, uint64_t height, const Array256_t& hash);

// Delete block by height
void deleteHeightHash(rocksdb::DB& db, uint64_t height);

// Get block hash by height
Array256_t getHeightHash(rocksdb::DB& db, uint64_t height);
