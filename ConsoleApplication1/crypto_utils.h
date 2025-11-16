#pragma once

array256_t bytesFromHex(const std::string& hex);

std::string hexFromBytes(const array256_t& bytes, const uint64_t& len);

array256_t sha256Of(const void* data, const uint64_t& len);

std::vector<uint8_t> serialiseTxInput(const TxInput& in, uint64_t version);

std::vector<uint8_t> serialiseUTXO(const UTXO& utxo, uint64_t version);

std::vector<uint8_t> serialiseTx(const Tx& tx, uint64_t version);

std::vector<uint8_t> serialiseBlock(Block& block);

TxInput formatTxInput(const uint8_t* data, uint64_t version);

UTXO formatUTXO(const uint8_t* data, uint64_t version);

Tx formatTx(const uint8_t* data, uint64_t version);

Block formatBlock(const uint8_t* serialisedBlock);
