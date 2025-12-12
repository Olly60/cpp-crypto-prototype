#pragma once
#include "crypto_utils.h"

void putUtxo(rocksdb::DB& db, const TxInput& txInput, const TxOutput& utxo);

void deleteUtxo(rocksdb::DB& db, const TxInput& txInput);

TxOutput getUtxo(rocksdb::DB& db, const TxInput& txInput);

std::unique_ptr<rocksdb::DB> openUtxoDb();

bool utxoInDb(rocksdb::DB& db, const TxInput& txInput);