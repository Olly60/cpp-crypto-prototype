#pragma once
#include <unordered_map>
#include "types.h"

bool processBlock(const Block& block,
    std::unordered_map<hash256_t, Block>& blockChain,
    std::unordered_map<UTXOKey, UTXO, UTXOKeyHash>& UTXOs);
