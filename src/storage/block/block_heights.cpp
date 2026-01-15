#include "storage/block/block_heights.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>

#include "tip.h"
#include "storage/storage_utils.h"
#include "storage/block/block_indexes.h"

namespace
{
    std::string makeHeightKey(const uint64_t height)
    {
        BytesBuffer heightBuffer;
        heightBuffer.writeU64(height);
        return std::string(heightBuffer.cdata(), heightBuffer.size());
    }

    std::string makeHashValue(const Array256_t& hash)
    {
        BytesBuffer valueBuf;
        valueBuf.writeArray256(hash);
        return std::string(valueBuf.cdata(), valueBuf.size());
    }

    Array256_t parseHashValue(const std::string& value)
    {
        Array256_t hash;
        std::copy_n(reinterpret_cast<const uint8_t*>(value.data()), 32, hash.begin());
        return hash;
    }
}

rocksdb::DB* heightsDb() {
    static rocksdb::DB* raw = []{
        rocksdb::Options options;
        options.create_if_missing = true;

        rocksdb::DB* db = nullptr;
        rocksdb::Status status = rocksdb::DB::Open(
            options,
            "block_heights",
            &db
        );

        if (!status.ok() || !db) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }

        return db;
    }();
    return raw;
}

std::optional<Array256_t>
tryGetHeightHash(uint64_t height)
{
    const std::string key = makeHeightKey(height);
    std::string value;
    BytesBuffer heightBuffer;
    heightBuffer.writeU64(height);

    auto status = heightsDb()->Get(
        rocksdb::ReadOptions(),
        key,
        &value
    );

    if (status.IsNotFound())
        return std::nullopt;

    if (!status.ok())
        throw std::runtime_error(
            "Height-hash read failed: " + status.ToString()
        );

    return parseHashValue(value);
}



void putHeightHashBatch(
    const std::vector<Array256_t>& hashes)
{
    rocksdb::WriteBatch batch;

    // Cache tip and pre-build all keys and values to ensure lifetime
    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(hashes.size());
    values.reserve(hashes.size());

    for (size_t i = 0; i < hashes.size(); ++i)
    {
        keys.push_back(makeHeightKey(tryGetBlockIndex(getTipHash())->height + i + 1));
        values.push_back(makeHashValue(hashes[i]));
    }

    for (size_t i = 0; i < keys.size(); ++i)
        batch.Put(keys[i], values[i]);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    auto status = heightsDb()->Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("Height-hash batch write failed: " + status.ToString());
}


void deleteHeightHashBatch(uint64_t amount)
{
    const uint64_t tip = tryGetBlockIndex(getTipHash())->height;
    if (amount > tip + 1)
        throw std::runtime_error("Count exceeds chain height");

    rocksdb::WriteBatch batch;

    // Pre-build keys to ensure lifetime
    std::vector<std::string> keys;
    keys.reserve(amount);
    for (uint64_t i = 0; i < amount; ++i)
        keys.push_back(makeHeightKey(tip - i));

    for (const auto& key : keys)
        batch.Delete(key);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    auto status = heightsDb()->Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("Height-hash batch delete failed: " + status.ToString());
}



