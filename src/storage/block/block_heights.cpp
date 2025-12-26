#include "storage/block/block_heights.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>

#include "tip.h"
#include "storage/storage_utils.h"

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
        BytesBuffer valueBuf;
        valueBuf.writeString(value);
        return valueBuf.readArray256();
    }
}

const std::filesystem::path BLOCK_HEIGHTS = "block_heights";

std::unique_ptr<rocksdb::DB> openHeightsDb()
{
    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(
        options,
        BLOCK_HEIGHTS,
        &raw
    );

    if (!status.ok() || !raw)
    {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}

std::optional<Array256_t>
tryGetHeightHash(rocksdb::DB& db, uint64_t height)
{
    const std::string key = makeHeightKey(height);
    std::string value;

    auto status = db.Get(
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
    rocksdb::DB& db,
    const std::vector<Array256_t>& hashes)
{
    rocksdb::WriteBatch batch;

    // Cache tip and pre-build all keys and values to ensure lifetime
    const uint64_t tip = getTipHeight();
    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(hashes.size());
    values.reserve(hashes.size());

    for (size_t i = 0; i < hashes.size(); ++i)
    {
        keys.push_back(makeHeightKey(tip + i + 1));
        values.push_back(makeHashValue(hashes[i]));
    }

    for (size_t i = 0; i < keys.size(); ++i)
        batch.Put(keys[i], values[i]);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    auto status = db.Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("Height-hash batch write failed: " + status.ToString());
}


void deleteHeightHashBatch(rocksdb::DB& db, uint64_t amount)
{
    const uint64_t tip = getTipHeight();
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

    auto status = db.Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("Height-hash batch delete failed: " + status.ToString());
}



