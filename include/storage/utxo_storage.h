#pragma once
#include "utxo_storage.h"
#include "crypto_utils.h"
#include <rocksdb/db.h>

#include "transaction.h"

rocksdb::DB* utxoDb();

std::optional<TxOutput> tryGetUtxo(
    const UTXOId& id);

// -------------------------------------------------
// Atomic block-level UTXO updates
// -------------------------------------------------

void applyUtxoBatch(

    const std::vector<UTXOId>& spends,
    const std::vector<std::pair<UTXOId, TxOutput>>& adds);