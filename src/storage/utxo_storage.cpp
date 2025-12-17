#include "crypto_utils.h"
#include <rocksdb/db.h>
#include "storage/file_utils.h"
#include "storage/utxo_storage.h"

namespace
{
    std::string makeUtxoKey(const TxInput& txInput) {
        return BytesBuffer(txInput.UTXOTxHash, txInput.UTXOOutputIndex).toString();
    }

    std::string makeUtxoValue(const TxOutput& utxo) {
        return BytesBuffer(utxo.amount, utxo.recipient).toString();
    }

    TxOutput parseUtxoValue(const std::string& value) {
        TxOutput utxo;
        BytesBuffer(value) >> utxo.amount >> utxo.recipient;
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
