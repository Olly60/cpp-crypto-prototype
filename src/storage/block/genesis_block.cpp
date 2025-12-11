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
    return getBlockHash(getGenesisBlock());
}
