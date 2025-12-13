#include "storage/block/block_heights.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>
#include <iostream>

#include "storage/file_utils.h"

namespace
{
    std::string makeHeightKey(const uint64_t& height)
    {
        std::string key;
        appendBytes(key, height);
        return key;
    }

    std::string makeHashValue(const Array256_t& hash)
    {
        std::string value;
        appendBytes(value, hash);
        return value;
    }
}

// Put block hash by height
void putBlockHeightHash(const uint64_t height, const Array256_t& hash) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::DB* db = nullptr;

    rocksdb::Status s = rocksdb::DB::Open(options, paths::blockHeights, &db);
    if (!s.ok()) {
        std::cerr << "Error opening DB: " << s.ToString() << "\n";
        return;
    }

    s = db->Put(rocksdb::WriteOptions(), makeHeightKey(height), makeHashValue(hash));
    if (!s.ok()) {
        std::cerr << "Error storing block: " << s.ToString() << "\n";
    }

    delete db; // closes DB
}

// Get block hash by height
Array256_t getBlockHeightHash(const uint64_t height) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::DB* db = nullptr;

    rocksdb::Status s = rocksdb::DB::Open(options, paths::blockHeights, &db);
    if (!s.ok()) {
        std::cerr << "Error opening DB: " << s.ToString() << "\n";
        return Array256_t{};
    }

    std::string value;
    s = db->Get(rocksdb::ReadOptions(), makeHeightKey(height), &value);
    if (!s.ok()) {
        std::cerr << "Error retrieving block: " << s.ToString() << "\n";
        delete db;
        return Array256_t{};
    }

    delete db;
    const std::span<const uint8_t> data(
    reinterpret_cast<const uint8_t*>(value.data()),
    value.size()
);
    Array256_t blockHash;
    takeBytesInto(blockHash, data);
    return blockHash;
}

// Delete block by height
void deleteBlockHeightHash(const uint64_t height) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::DB* db = nullptr;

    rocksdb::Status s = rocksdb::DB::Open(options, paths::blockHeights, &db);
    if (!s.ok()) {
        std::cerr << "Error opening DB: " << s.ToString() << "\n";
        return;
    }

    s = db->Delete(rocksdb::WriteOptions(), makeHeightKey(height));
    if (!s.ok()) {
        std::cerr << "Error deleting block: " << s.ToString() << "\n";
    }

    delete db;
}
