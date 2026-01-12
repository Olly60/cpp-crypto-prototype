#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>
#include "storage/storage_utils.h"
#include "storage/block/block_indexes.h"

namespace
{
    // Serialize hash key
    std::string makeHashKey(const Array256_t& hash)
    {
        BytesBuffer buf;
        buf.writeArray256(hash);
        return std::string(buf.cdata(), buf.size());

    }

    // Serialize BlockIndexValue
    std::string makeIndexValue(const BlockIndexValue& index)
    {
        BytesBuffer buf;
        buf.writeU64(index.height);
        buf.writeArray256(index.chainWork);
        return std::string(buf.cdata(), buf.size());
    }

    // Deserialize BlockIndexValue
    BlockIndexValue parseIndexValue(const std::string& value)
    {
        BytesBuffer buf;
        buf.writeString(value);
        BlockIndexValue result;
        result.height = buf.readU64();
        result.chainWork = buf.readArray256();
        return result;
    }
}

rocksdb::DB* blockIndexesDb() {
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

// Put block index
void putBlockIndexBatch(
    const std::vector<Array256_t>& hashes,
    const std::vector<BlockIndexValue>& values)
{
    if (hashes.size() != values.size())
        throw std::runtime_error("hash/value size mismatch");

    rocksdb::WriteBatch batch;

    // Keys and values must outlive db.Write()
    std::vector<std::string> keys;
    std::vector<std::string> vals;
    keys.reserve(hashes.size());
    vals.reserve(values.size());

    for (size_t i = 0; i < hashes.size(); ++i)
    {
        keys.push_back(makeHashKey(hashes[i]));
        vals.push_back(makeIndexValue(values[i]));
    }

    for (size_t i = 0; i < keys.size(); ++i)
        batch.Put(keys[i], vals[i]);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    const auto s = blockIndexesDb()->Write(wo, &batch);
    if (!s.ok())
        throw std::runtime_error(
            "Batch put block index failed: " + s.ToString()
        );
}


// Delete block index
void batchDeleteBlockIndex(
    const std::vector<Array256_t>& hashes)
{
    rocksdb::WriteBatch batch;

    // Keys must outlive db.Write()
    std::vector<std::string> keys;
    keys.reserve(hashes.size());

    for (const auto& hash : hashes)
        keys.push_back(makeHashKey(hash));

    for (const auto& key : keys)
        batch.Delete(key);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    const auto s = blockIndexesDb()->Write(wo, &batch);
    if (!s.ok())
        throw std::runtime_error(
            "Batch delete block index failed: " + s.ToString()
        );
}



// Get block index
std::optional<BlockIndexValue>
tryGetBlockIndex(const Array256_t& hash)
{
    const std::string key = makeHashKey(hash);
    std::string value;

    auto s = blockIndexesDb()->Get(
        rocksdb::ReadOptions(),
        key,
        &value
    );

    if (s.IsNotFound())
        return std::nullopt;

    if (!s.ok())
        throw std::runtime_error(
            "Block index read failed: " + s.ToString()
        );

    return parseIndexValue(value);
}





