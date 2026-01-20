#include "crypto_utils.h"
#include "storage/storage_utils.h"
#include "storage/utxo_storage.h"

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

#include "transaction.h"

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

    UTXOId parseUtxoKey(const std::string& key)
    {
        UTXOId utxoId;
        BytesBuffer buf;
        buf.insertBytes(key.data(), key.data() + key.size());

        utxoId.UTXOTxHash = buf.readArray256();

        utxoId.UTXOOutputIndex = buf.readU64();

        return utxoId;
    }

    TxOutput parseUtxoValue(const std::string& value)
    {
        TxOutput out;
        BytesBuffer buf;
        buf.insertBytes(value.data(), value.data() + value.size());

        out.amount = buf.readU64();

        out.recipient = buf.readArray256();

        return out;
    }


}

// -------------------------------------------------
// Single-UTXO operations (non-throwing where possible)
// -------------------------------------------------

rocksdb::DB* utxoDb()
{
    static rocksdb::DB* raw = []
    {
        rocksdb::Options options;
        options.create_if_missing = true;

        rocksdb::DB* db = nullptr;
        rocksdb::Status status = rocksdb::DB::Open(
            options,
            "utxos",
            &db
        );

        if (!status.ok() || !db)
        {
            throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
        }

        return db;
    }();
    return raw;
}


std::optional<TxOutput>
tryGetUtxo(const UTXOId& id)
{
    const std::string key = makeUtxoKey(id.UTXOTxHash, id.UTXOOutputIndex);
    std::string value;

    auto status = utxoDb()->Get(
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

    const std::vector<UTXOId>& spends,
    const std::vector<std::pair<UTXOId, TxOutput>>& adds)
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

    auto status = utxoDb()->Write(wo, &batch);
    if (!status.ok())
        throw std::runtime_error("UTXO batch write failed: " + status.ToString());
}

std::unordered_set<UTXOId, UTXOIdHash> getUtxosForRecipient(const Array256_t& recipient)
{
    std::unordered_set<UTXOId , UTXOIdHash> result;
    std::unique_ptr<rocksdb::Iterator> it(utxoDb()->NewIterator(rocksdb::ReadOptions{}));

    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        const std::string& key = it->key().ToString();
        const std::string& value = it->value().ToString();

        UTXOId utxoId = parseUtxoKey(key);
        TxOutput txOutput = parseUtxoValue(value);

        if (txOutput.recipient == recipient)
        {
            result.insert(utxoId);
        }
    }

    if (!it->status().ok())
    {
        throw std::runtime_error("UTXO iteration failed: " + it->status().ToString());
    }

    return result;
}

