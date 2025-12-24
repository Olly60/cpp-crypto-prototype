#include "storage/block/block_heights.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <array>
#include <string>
#include "storage/file_utils.h"

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
