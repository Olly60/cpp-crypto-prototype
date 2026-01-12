#pragma once
#include "utxo_storage.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>

rocksdb::DB* utxoDb();

std::optional<TxOutput> tryGetUtxo(
    const TxInput& input);

// -------------------------------------------------
// Atomic block-level UTXO updates
// -------------------------------------------------

void applyUtxoBatch(

    const std::vector<TxInput>& spends,
    const std::vector<std::pair<TxInput, TxOutput>>& adds);