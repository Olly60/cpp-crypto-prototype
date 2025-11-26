#pragma once

bool verifyMerkleRoot(const Block& block);

bool verifyBlockHash(const Block& block, const Array256_t& expectedHash);

bool validateTx(const Tx& tx);

static bool validateBlock(Block block);
