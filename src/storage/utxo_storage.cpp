#include "crypto_utils.h"
#include "storage/file_utils.h"
#include "storage/utxo_storage.h"

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

namespace
{
    // -------------------------------
    // UTXO key (txid + output index)
    // -------------------------------
    std::string makeUtxoKey(const Array256_t& txHash, uint64_t index)
    {
        BytesBuffer buf;
        buf.writeArray256(txHash);
        buf.writeU64(index);
        return std::string(buf.cdata(), buf.size());
    }

    // -------------------------------
    // UTXO value (amount + recipient)
    // -------------------------------
    std::string makeUtxoValue(const TxOutput& utxo)
    {
        BytesBuffer buf;
        buf.writeU64(utxo.amount);
        buf.writeArray256(utxo.recipient);
        return std::string(buf.cdata(), buf.size());
    }

    TxOutput parseUtxoValue(const std::string& value)
    {
        BytesBuffer buf;
        buf.writeString(value);

        TxOutput out;
        out.amount    = buf.readU64();
        out.recipient = buf.readArray256();
        return out;
    }

    inline rocksdb::Slice slice(const std::string& s)
    {
        return rocksdb::Slice(s.data(), s.size());
    }
}

// -------------------------------------------------
// Single-UTXO operations (non-throwing where possible)
// -------------------------------------------------

std::unique_ptr<rocksdb::DB> openUtxoDb()
{
    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(
        options,
        "block_heights",
        &raw
    );

    if (!status.ok() || !raw)
    {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}

std::optional<TxOutput> tryGetUtxo(rocksdb::DB& db,
    const TxInput& input)
{
    std::string value;
    auto status = db.Get(
        rocksdb::ReadOptions(),
        slice(makeUtxoKey(input.UTXOTxHash, input.UTXOOutputIndex)),
        &value
    );

    if (!status.ok()) return std::nullopt;

    return parseUtxoValue(value);
}

// -------------------------------------------------
// Atomic block-level UTXO updates
// -------------------------------------------------

void applyUtxoBatch(
    rocksdb::DB& db,
    const std::vector<TxInput>& spends,
    const std::vector<std::pair<TxInput, TxOutput>>& adds)
{
    rocksdb::WriteBatch batch;

    // Spend inputs
    for (const TxInput& in : spends)
    {
        batch.Delete(
            makeUtxoKey(in.UTXOTxHash, in.UTXOOutputIndex)
        );
    }

    // Add outputs
    for (const auto& [in, out] : adds)
    {
        batch.Put(
            makeUtxoKey(in.UTXOTxHash, in.UTXOOutputIndex),
            makeUtxoValue(out)
        );
    }

    rocksdb::WriteOptions wo;
    wo.sync = false;

    auto status = db.Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("UTXO batch write failed");
}
