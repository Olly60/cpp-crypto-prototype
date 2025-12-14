#include "crypto_utils.h"
#include <rocksdb/db.h>
#include "storage/file_utils.h"
#include "storage/utxo_storage.h"

namespace
{
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

    TxOutput parseUtxoValue(const std::string& value) {
        TxOutput utxo;
        size_t offset = 0;
        const auto data = std::span(
            reinterpret_cast<const uint8_t*>(value.data()), value.size());
        takeBytesInto(utxo.amount, data, offset);
        takeBytesInto(utxo.recipient, data, offset);
        return utxo;
    }
}

// RocksDB UTXO operations
void putUtxo(rocksdb::DB& db, const TxInput& txInput, const TxOutput& utxo) {
    if (!db.Put(rocksdb::WriteOptions(),
                rocksdb::Slice(makeUtxoKey(txInput)),
                rocksdb::Slice(makeUtxoValue(utxo)))
             .ok()) {
        throw std::runtime_error("Failed to put UTXO");
             }
}

void deleteUtxo(rocksdb::DB& db, const TxInput& txInput) {
    if (!db.Delete(rocksdb::WriteOptions(),
                   rocksdb::Slice(makeUtxoKey(txInput)))
             .ok()) {
        throw std::runtime_error("Failed to delete UTXO");
             }
}

TxOutput getUtxo(rocksdb::DB& db, const TxInput& txInput) {
    std::string value;
    if (!db.Get(rocksdb::ReadOptions(),
                rocksdb::Slice(makeUtxoKey(txInput)),
                &value)
             .ok()) {
        throw std::runtime_error("UTXO not found");
             }
    return parseUtxoValue(value);
}
