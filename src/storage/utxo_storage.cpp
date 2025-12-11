#include "crypto_utils.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>

namespace {

    std::string makeUtxoKey(const TxInput& txInput) {
        std::string key;
        appendBytes(key, txInput.UTXOTxHash);
        appendBytes(key, txInput.UTXOOutputIndex);
        return key;
    }

    std::string makeUtxoValue(const TxOutput& utxo) {
        std::string value;
        appendBytes(value, utxo.amount);
        appendBytes(value, utxo.recipient);
        return value;
    }

    TxOutput formatUtxoValue(const std::string& value) {
        TxOutput utxo;
        size_t offset = 0;
        std::span<const uint8_t> data(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.size()
        );
        takeBytesInto(utxo.amount, data, offset);
        takeBytesInto(utxo.recipient, data, offset);
        return utxo;
    }

} // namespace


// ============================================================================
// RocksDB UTXO operations
// ============================================================================

void putUtxo(rocksdb::DB& db, const TxInput& txInput, const TxOutput& utxo) {
    std::string key = makeUtxoKey(txInput);
    std::string value = makeUtxoValue(utxo);

    rocksdb::Status status = db.Put(
        rocksdb::WriteOptions(),
        rocksdb::Slice(key),
        rocksdb::Slice(value)
    );

    if (!status.ok()) {
        throw std::runtime_error("Failed to put UTXO: " + status.ToString());
    }
}

void deleteUtxo(rocksdb::DB& db, const TxInput& txInput) {
    std::string key = makeUtxoKey(txInput);

    rocksdb::Status status = db.Delete(
        rocksdb::WriteOptions(),
        rocksdb::Slice(key)
    );

    if (!status.ok()) {
        throw std::runtime_error("Failed to delete UTXO: " + status.ToString());
    }
}

TxOutput getUtxo(rocksdb::DB& db, const TxInput& txInput) {
    std::string key = makeUtxoKey(txInput);
    std::string value;

    rocksdb::Status status = db.Get(
        rocksdb::ReadOptions(),
        rocksdb::Slice(key),
        &value
    );

    if (!status.ok()) {
        throw std::runtime_error("UTXO not found: " + status.ToString());
    }

    return formatUtxoValue(value);
}

std::unique_ptr<rocksdb::DB> openUtxoDb() {
    fs::create_directories(paths::utxo);

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(
        options,
        (paths::utxo / "rocksdb").string(),
        &raw
    );

    if (!status.ok() || !raw) {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}

bool utxoInDb(rocksdb::DB& db, const TxInput& txInput) {
    std::string key = makeUtxoKey(txInput);
    std::string value;

    rocksdb::Status status = db.Get(
        rocksdb::ReadOptions(),
        rocksdb::Slice(key),
        &value
    );

    return status.ok();
}
