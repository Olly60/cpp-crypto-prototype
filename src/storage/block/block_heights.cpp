#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include "crypto_utils.h"
#include "file-utils"

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

    Array256_t formatHashValue(const std::string& value)
    {
        Array256_t hash;
        takeBytesInto(
            hash,
            std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(value.data()),
                value.size()
            )
        );
        return hash;
    }
} // namespace


void putHeightHash(rocksdb::DB& db, const uint64_t& height, const Array256_t& hash)
{
    std::string key = makeHeightKey(height);
    std::string value = makeHashValue(hash);

    rocksdb::Status status = db.Put(
        rocksdb::WriteOptions(),
        rocksdb::Slice(key),
        rocksdb::Slice(value)
    );

    if (!status.ok())
    {
        throw std::runtime_error("RocksDB put failed: " + status.ToString());
    }
}


void deleteHeightHash(rocksdb::DB& db, const uint64_t& height)
{
    std::string key = makeHeightKey(height);

    rocksdb::Status status = db.Delete(
        rocksdb::WriteOptions(),
        rocksdb::Slice(key)
    );

    if (!status.ok())
    {
        throw std::runtime_error("RocksDB delete failed: " + status.ToString());
    }
}


Array256_t getHeightHash(rocksdb::DB& db, const uint64_t& height)
{
    std::string key = makeHeightKey(height);
    std::string value;

    rocksdb::Status status = db.Get(
        rocksdb::ReadOptions(),
        key,
        &value
    );

    if (!status.ok())
    {
        throw std::runtime_error("Height hash not found: " + status.ToString());
    }

    return formatHashValue(value);
}


std::unique_ptr<rocksdb::DB> openHeightHash()
{
    fs::create_directories(paths::heightDb);

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(
        options,
        (paths::heightDb / "rocksdb").string(),
        &raw
    );

    if (!status.ok() || !raw)
    {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}


bool HeightHashInDb(rocksdb::DB& db, const uint64_t& height)
{
    std::string key = makeHeightKey(height);
    std::string value;

    rocksdb::Status status = db.Get(
        rocksdb::ReadOptions(),
        key,
        &value
    );

    return status.ok();
}
