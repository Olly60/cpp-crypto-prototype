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
        BlockIndexValue idx;
        BytesBuffer buf;
        buf.insertBytes(value.data(), value.data() + value.size());

        idx.height = buf.readU64();

        idx.chainWork = buf.readArray256();

        return idx;
    }
}

rocksdb::DB* blockIndexesDb() {
    static rocksdb::DB* raw = []{
        rocksdb::Options options;
        options.create_if_missing = true;

        rocksdb::DB* db = nullptr;
        rocksdb::Status status = rocksdb::DB::Open(
            options,
            "block_indexes",
            &db
        );

        if (!status.ok() || !db) {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }

        return db;
    }();
    return raw;
}

void putBlockIndexBatch(
    const std::vector<std::pair<Array256_t, BlockIndexValue>>& indexes)
{
    rocksdb::WriteBatch batch;

    std::vector<std::string> keys;
    std::vector<std::string> vals;
    keys.reserve(indexes.size());
    vals.reserve(indexes.size());

    for (auto& index : indexes)
    {
        keys.push_back(makeHashKey(index.first));
        vals.push_back(makeIndexValue(index.second));
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





