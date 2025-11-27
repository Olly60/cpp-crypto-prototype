#pragma once

bool verifyMerkleRoot(const Block& block);

bool verifyBlockHash(const Block& block, const Array256_t& expectedHash);

bool verifyTx(const Tx& tx);

bool verifyBlock(Block block);
