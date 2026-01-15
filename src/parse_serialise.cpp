#include "parse_serialise.h"

// ----------------------------------------
// Tx
// ----------------------------------------
BytesBuffer serialiseTx(const Tx& tx)
{
    BytesBuffer txBytes;

    // Version
    txBytes.writeU64(tx.version);

    // Inputs amount
    txBytes.writeU64(tx.txInputs.size());

    // Inputs
    for (const auto& txInput : tx.txInputs)
    {
        txBytes.writeArray256(txInput.UTXOTxHash);
        txBytes.writeU64(txInput.UTXOOutputIndex);
        txBytes.writeArray512(txInput.signature);
    }

    // Outputs amount
    txBytes.writeU64(tx.txOutputs.size());

    // Outputs
    for (const auto& txOutput : tx.txOutputs)
    {
        txBytes.writeU64(txOutput.amount);
        txBytes.writeArray256(txOutput.recipient);
    }

    return txBytes;
}

Tx parseTx(BytesBuffer& txBytes)
{
    Tx tx;

    // Tx Version
    tx.version = txBytes.readU64();

    // Input amount
    uint64_t inputAmount = txBytes.readU64();
    tx.txInputs.reserve(inputAmount);

    // Read inputs
    for (uint64_t i = 0; i < inputAmount; i++)
    {
        TxInput txInput;
        txInput.UTXOTxHash = txBytes.readArray256();
        txInput.UTXOOutputIndex = txBytes.readU64();
        txInput.signature = txBytes.readArray512();
        tx.txInputs.push_back(txInput);
    }

    // Output amount
    uint64_t outputAmount = txBytes.readU64();
    tx.txOutputs.reserve(outputAmount);

    // Read outputs
    for (uint64_t i = 0; i < outputAmount; i++)
    {
        TxOutput txOutput;
        txOutput.amount = txBytes.readU64();
        txOutput.recipient = txBytes.readArray256();
        tx.txOutputs.push_back(txOutput);
    }

    return tx;
}

// ----------------------------------------
// BlockHeader
// ----------------------------------------
BytesBuffer serialiseBlockHeader(const BlockHeader& header)
{
    BytesBuffer headerBytes;
    headerBytes.writeU64(header.version);
    headerBytes.writeArray256(header.prevBlockHash);
    headerBytes.writeArray256(header.merkleRoot);
    headerBytes.writeU64(header.timestamp);
    headerBytes.writeArray256(header.difficulty);
    headerBytes.writeArray256(header.nonce);
    return headerBytes;
}

BlockHeader parseBlockHeader(BytesBuffer& headerBytes)
{
    BlockHeader header;
    header.version = headerBytes.readU64();
    header.prevBlockHash = headerBytes.readArray256();
    header.merkleRoot = headerBytes.readArray256();
    header.timestamp = headerBytes.readU64();
    header.difficulty = headerBytes.readArray256();
    header.nonce = headerBytes.readArray256();
    return header;
}

// ----------------------------------------
// Block
// ----------------------------------------
BytesBuffer serialiseBlock(const ChainBlock& block)
{

    BytesBuffer blockBytes;

    // Header
    blockBytes.writeBytesBuffer(serialiseBlockHeader(block.header));

    // Transaction amount
    blockBytes.writeU64(block.txs.size());

    // Transactions
    for (const auto& tx : block.txs)
    {
        blockBytes.writeBytesBuffer(serialiseTx(tx));
    }

    return blockBytes;
}

ChainBlock parseBlock(BytesBuffer& blockBytes)
{
    ChainBlock block;

    // Header
    block.header = parseBlockHeader(blockBytes);

    // Transaction amount
    uint64_t txCount = blockBytes.readU64();
    block.txs.reserve(txCount);

    // Transactions
    for (uint64_t i = 0; i < txCount; i++)
    {
        block.txs.push_back(parseTx(blockBytes));
    }

    return block;
}
