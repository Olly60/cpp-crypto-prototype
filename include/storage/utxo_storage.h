#pragma once
#include "utxo_storage.h"
#include "crypto_utils.h"
#include <filesystem>
#include <fstream>
#include "storage/utxo_storage.h"
#include <rocksdb/db.h>

void putUtxo(rocksdb::DB& db, const TxInput& txInput, const TxOutput& utxo);

void deleteUtxo(rocksdb::DB& db, const TxInput& txInput);

TxOutput getUtxo(rocksdb::DB& db, const TxInput& txInput);

std::unique_ptr<rocksdb::DB> openUtxoDb();

bool utxoInDb(rocksdb::DB& db, const TxInput& txInput);