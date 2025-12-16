#include "storage/block/block_heights.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>
#include "storage/file_utils.h"

namespace
{
    std::string makeHeightKey(const uint64_t& height)
    {
        std::string key;
        BytesBuffer(height) >> key;
        return key;
    }

    std::string makeHashValue(const Array256_t& hash)
    {
        std::string value;
        BytesBuffer(hash) >> value;
        return value;
    }

    Array256_t parseHashValue(const std::string& value)
    {
        Array256_t hash;
        BytesBuffer(value) >> hash;
        return hash;
    }
}

// Put block hash by height
void putHeightHash(rocksdb::DB& db, const uint64_t height, const Array256_t& hash)
{
    if (!db.Put(rocksdb::WriteOptions(),
                rocksdb::Slice(makeHeightKey(height)),
                rocksdb::Slice(makeHashValue(hash)))
           .ok())
    {
        throw std::runtime_error("Failed to put height hash");
    }
}


// Delete block by height
void deleteHeightHash(rocksdb::DB& db, const uint64_t height)
{
    if (!db.Delete(rocksdb::WriteOptions(), rocksdb::Slice(makeHeightKey(height))).ok())
    {
        throw std::runtime_error("Failed to delete height hash");
    }
}


// Get block hash by height
Array256_t getHeightHash(rocksdb::DB& db, const uint64_t height)
{
    std::string value;
    if (!db.Get(rocksdb::ReadOptions(),
                rocksdb::Slice(makeHeightKey(height)),
                &value)
           .ok())
    {
        throw std::runtime_error("UTXO not found");
    }
    return parseHashValue(value);
}
