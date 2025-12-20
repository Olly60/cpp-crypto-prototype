#include "crypto_utils.h"
#include <rocksdb/db.h>
#include "storage/file_utils.h"
#include "storage/utxo_storage.h"

namespace
{
    std::string makeUtxoKey(const TxInput& txInput) {
        BytesBuffer txInputBuf;
        txInputBuf.writeArray256(txInput.UTXOTxHash);
        txInputBuf.writeU64(txInput.UTXOOutputIndex);
        return txInputBuf.toString();
    }

    std::string makeUtxoValue(const TxOutput& utxo) {
        BytesBuffer valueBuf;
        valueBuf.writeU64(utxo.amount);
        valueBuf.writeArray256(utxo.recipient);
        return valueBuf.toString();
    }

    TxOutput parseUtxoValue(const std::string& value) {
        BytesBuffer valueBuf;
        valueBuf.writeString(value);
        TxOutput txOutput;
        txOutput.amount = valueBuf.readU64();
        txOutput.recipient = valueBuf.readArray256();
        return txOutput;
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
