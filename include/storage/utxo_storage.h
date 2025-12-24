#pragma once
#include "utxo_storage.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>

std::unique_ptr<rocksdb::DB> openUtxoDb();

bool utxoExists(
    rocksdb::DB& db,
    const TxInput& input);

bool tryGetUtxo(rocksdb::DB& db,
    TxOutput& out,
    const TxInput& input);

// -------------------------------------------------
// Atomic block-level UTXO updates
// -------------------------------------------------

void applyUtxoBatch(
    rocksdb::DB& db,
    const std::vector<TxInput>& spends,
    const std::vector<std::pair<TxInput, TxOutput>>& adds);