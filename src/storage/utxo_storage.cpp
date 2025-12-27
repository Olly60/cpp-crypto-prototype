#include "crypto_utils.h"
#include "storage/storage_utils.h"
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

std::optional<TxOutput>
tryGetUtxo(rocksdb::DB& db, const TxInput& input)
{
    const std::string key = makeUtxoKey(input.UTXOTxHash, input.UTXOOutputIndex);
    std::string value;

    auto status = db.Get(
        rocksdb::ReadOptions(),
        key,
        &value
    );

    if (status.IsNotFound())
        return std::nullopt;

    if (!status.ok())
        throw std::runtime_error("UTXO read failed: " + status.ToString());

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

    // Pre-build keys and values to ensure lifetime
    std::vector<std::string> spendKeys;
    spendKeys.reserve(spends.size());
    for (const auto& in : spends)
        spendKeys.push_back(makeUtxoKey(in.UTXOTxHash, in.UTXOOutputIndex));

    std::vector<std::string> addKeys;
    std::vector<std::string> addValues;
    addKeys.reserve(adds.size());
    addValues.reserve(adds.size());

    for (const auto& [in, out] : adds)
    {
        addKeys.push_back(makeUtxoKey(in.UTXOTxHash, in.UTXOOutputIndex));
        addValues.push_back(makeUtxoValue(out));
    }

    // Apply deletes
    for (const auto& key : spendKeys)
        batch.Delete(key);

    // Apply adds
    for (size_t i = 0; i < addKeys.size(); ++i)
        batch.Put(addKeys[i], addValues[i]);

    rocksdb::WriteOptions wo;
    wo.sync = false;

    auto status = db.Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("UTXO batch write failed: " + status.ToString());
}

