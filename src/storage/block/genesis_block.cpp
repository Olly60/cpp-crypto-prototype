#include "storage/block/genesis_block.h"
#include <filesystem>

#include "block_work.h"
#include "crypto_utils.h"
#include "tip.h"
#include "storage/storage_utils.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "../../../include/block.h"

// Genesis block
ChainBlock getGenesisBlock()
{
    // Genesis transaction
    TxOutput genesisOutput;
    genesisOutput.amount = 5000000000; // amount

    Tx genesisTx;
    genesisTx.version = 1;
    genesisTx.txOutputs.push_back(genesisOutput);

    // Genesis block
    BlockHeader header;

    ChainBlock genesisBlock;
    genesisBlock.txs.push_back(genesisTx);

    header.merkleRoot = getMerkleRoot(genesisBlock.txs);
    genesisBlock.header = header;

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
    rocksdb::WriteOptions wo;
    std::string key(1, 0);
    std::string value(reinterpret_cast<const char*>(genesisBlockHash.data()), genesisBlockHash.size());
    rocksdb::Status s = heightsDb()->Put(wo, rocksdb::Slice(key), rocksdb::Slice(value));
    if (!s.ok()) throw std::runtime_error(s.ToString());

    // Setup Index
    BlockIndexValue blockIndex;
    blockIndex.chainWork = getBlockWork(genesisBlock.header.difficulty);
    blockIndex.height = 0;
    putBlockIndexBatch({genesisBlockHash}, {blockIndex});

    // Write block file
    writeFileTrunc(getBlockFilePath(genesisBlockHash), serialiseBlock(genesisBlock));

    // Genesis utxo
    applyUtxoBatch( {}, {std::pair<TxInput, TxOutput>({getTxHash(genesisBlock.txs[0]),0}, genesisBlock.txs[0].txOutputs[0])});
}
