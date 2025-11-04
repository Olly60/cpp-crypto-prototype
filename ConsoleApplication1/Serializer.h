#pragma once
#include <vector>
#include "types.h"
#include "crypto.h"

void hashTransaction(hash256_t& out, const Transaction& tx);
void serializeBlockHeader(const BlockHeader& header, std::array<uint8_t, 96>& out);
#pragma once
