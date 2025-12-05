#pragma once
#include <leveldb/db.h>
#include <memory>

// Block

Array256_t getBlockchainTip();

Array256_t getGenesisBlockHash();

void addBlock(const Block& block);

void undoBlock();

std::vector<uint8_t> readBlockFile(const Array256_t& blockHash);

std::vector<uint8_t> readBlockFileHeader(const Array256_t& blockHash);

bool blockExists(const Array256_t& blockHash);

Block getBlock(Array256_t blockHash);

BlockHeader getBlockHeaderByHash(Array256_t blockHash);

// Utxo 

TxOutput getUtxo(leveldb::DB& db, const TxInput& txInput);

std::unique_ptr<leveldb::DB> openUtxoDb();

bool utxoInDb(leveldb::DB& db, const TxInput& txInput);


