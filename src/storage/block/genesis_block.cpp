#include "genesis_block.h"

#pragma once
#include <filesystem>

#include "crypto_utils.h"
#include "tip.h"
#include "storage/storage_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"

// Genesis block
ChainBlock getGenesisBlock()
{
    // Genesis transaction
    TxOutput genesisOutput{
        5000000000, // amount
    };

    Tx genesisTx{
        1, // version
        {}, // no inputs
        {genesisOutput}
    };

    // Genesis block
    BlockHeader header;
    header.merkleRoot = getTxHash(genesisTx);

    ChainBlock genesisBlock{
        header,
        {genesisTx}
    };

    return genesisBlock;
}

Array256_t getGenesisBlockHash()
{
    return getBlockHeaderHash(getGenesisBlock().header);
}

void initGenesisBlock()
{
    // If genesis block exists skip
    if (std::filesystem::exists(getBlockFilePath(getGenesisBlockHash()))) return;

    auto genesisBlock = getGenesisBlock();
    auto genesisBlockHash = getGenesisBlockHash();

    // Setup tip file
    BytesBuffer hashBuf;
    hashBuf.writeArray256(genesisBlockHash);
    writeFileTrunc(TIP, hashBuf);

    // Setup height
    auto heightsDb = openHeightsDb();
    rocksdb::WriteOptions wo;
    heightsDb->Put(wo , rocksdb::Slice(0), rocksdb::Slice(genesisBlockHash.));

    // Setup Index
    auto blockIndexesDb = openBlockIndexesDb();
    BlockIndexValue blockIndex;
    blockIndex.chainWork = getBlockWork(genesisBlock.header.difficulty);
    blockIndex.height = 0;
    putBlockIndexBatch(*blockIndexesDb, {genesisBlockHash}, {blockIndex});

    // Write block file
    writeFileTrunc(getBlockFilePath(genesisBlockHash), serialiseBlock(genesisBlock));

    // Genesis utxo
    auto utxoDb = openUtxoDb();
    applyUtxoBatch(*utxoDb, {}, {std::pair<TxInput, TxOutput>({getTxHash(genesisBlock.txs[0]),0}, genesisBlock.txs[0].txOutputs[0])});
}
