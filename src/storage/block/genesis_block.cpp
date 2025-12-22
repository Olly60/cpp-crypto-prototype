#include "storage/block/genesis_block.h"
#include "crypto_utils.h"

Block getGenesisBlock()
{
    // Genesis transaction
    TxOutput genesisOutput{
        0, // amount
        {} // recipient (empty for genesis)
    };

    Tx genesisTx{
        1, // version
        {}, // no inputs
        {genesisOutput}
    };

    // Genesis block
    BlockHeader header;
    header.merkleRoot = getTxHash(genesisTx);

    Block genesisBlock{
        header,
        {genesisTx}
    };

    return genesisBlock;
}

Array256_t getGenesisBlockHash()
{
    return getBlockHeaderHash(getGenesisBlock().header);
}
