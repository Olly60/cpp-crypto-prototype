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
        return buf.toString();
    }

    // -------------------------------
    // UTXO value (amount + recipient)
    // -------------------------------
    std::string makeUtxoValue(const TxOutput& utxo)
    {
        BytesBuffer buf;
        buf.writeU64(utxo.amount);
        buf.writeArray256(utxo.recipient);
        return buf.toString();
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

bool utxoExists(
    rocksdb::DB& db,
    const TxInput& input)
{
    std::string value;
    auto status = db.Get(
        rocksdb::ReadOptions(),
        slice(makeUtxoKey(input.UTXOTxHash, input.UTXOOutputIndex)),
        &value
    );

    return status.ok();
}

bool tryGetUtxo(rocksdb::DB& db,
    TxOutput& out,
    const TxInput& input)
{
    std::string value;
    auto status = db.Get(
        rocksdb::ReadOptions(),
        slice(makeUtxoKey(input.UTXOTxHash, input.UTXOOutputIndex)),
        &value
    );

    if (!status.ok())
        return false;

    out = parseUtxoValue(value);
    return true;
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
