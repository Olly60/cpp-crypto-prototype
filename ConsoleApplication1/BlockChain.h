#pragma once
#include <unordered_map>
#include "types.h"
#include "crypto.h"
#include "serializer.h"

bool processBlock(const Block& block,
    std::unordered_map<hash256_t, Block>& blockChain,
    std::unordered_map<UTXOKey, UTXO, UTXOKeyHash>& UTXOs);
#pragma once
