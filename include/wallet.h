#pragma once
#include <unordered_map>
#include <unordered_set>
#include "crypto_utils.h"
#include "transaction.h"

inline std::unordered_map<Array256_t, std::unordered_set<UTXOId, UTXOIdHash>, Array256Hash> wallets;

void storeWallets();

void loadWallets();