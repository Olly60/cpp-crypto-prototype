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
        return BytesBuffer(hash).toStringHex();

    }

    // Serialize BlockIndexValue
    std::string makeIndexValue(const BlockIndexValue& index)
    {
        BytesBuffer indexBytes;
        std::string value;
        BytesBuffer(index.height, index.chainwork) >> value;
        return value;
    }

    // Deserialize BlockIndexValue
    BlockIndexValue parseIndexValue(const std::string& value)
    {
        if (value.size() < sizeof(uint64_t) + Array256_t{}.size())
            throw std::runtime_error("Invalid BlockIndexValue size");

        BlockIndexValue result{};
        size_t offset = 0;
        parseBytesInto(result.height, std::span(
            reinterpret_cast<const uint8_t*>(value.data()), value.size()), offset);
        parseBytesInto(result.chainwork, std::span(
            reinterpret_cast<const uint8_t*>(value.data()), value.size()), offset);
        return result;
    }
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
