#include "genesis_block.h"

#pragma once
#include "crypto_utils.h"

// Genesis block
Block getGenesisBlock()
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

void initGenesisBlock()
{

}
