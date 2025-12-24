#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>
#include "storage/file_utils.h"
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

const std::filesystem::path BLOCK_INDEXES = "block_indexes";
std::unique_ptr<rocksdb::DB> openBlockIndexesDb()
{

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(
        options,
        "block_indexes",
        &raw
    );

    if (!status.ok() || !raw)
    {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}

// Put block index
void putBlockIndex(rocksdb::DB& db, const Array256_t& hash, const BlockIndexValue& value)
{
    const rocksdb::Status s = db.Put(rocksdb::WriteOptions(),
                               rocksdb::Slice(makeHashKey(hash)),
                               rocksdb::Slice(makeIndexValue(value)));
    if (!s.ok()) {
        throw std::runtime_error("Failed to put block index: " + s.ToString());
    }
}

// Delete block index
void deleteBlockIndex(rocksdb::DB& db, const Array256_t& hash)
{
    if (const rocksdb::Status s = db.Delete(rocksdb::WriteOptions(), rocksdb::Slice(makeHashKey(hash))); !s.ok()) {
        throw std::runtime_error("Failed to delete block index: " + s.ToString());
    }
}

// Get block index
BlockIndexValue getBlockIndex(rocksdb::DB& db, const Array256_t& hash)
{
    std::string value;
    const rocksdb::Status s = db.Get(rocksdb::ReadOptions(),
                               rocksdb::Slice(makeHashKey(hash)),
                               &value);
    if (!s.ok()) {
        throw std::runtime_error("Block index not found: " + s.ToString());
    }
    return parseIndexValue(value);
}


