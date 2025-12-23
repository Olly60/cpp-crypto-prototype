#include "crypto_utils.h"

// ----------------------------------------
// TxInput
// ----------------------------------------
BytesBuffer serialiseTxInput(const TxInput& txInput)
{
    BytesBuffer serialisedTx;
    serialisedTx.writeArray256(txInput.UTXOTxHash);
    serialisedTx.writeU64(txInput.UTXOOutputIndex);
    serialisedTx.writeArray512(txInput.signature);
    return serialisedTx;
}

TxInput parseTxInput(BytesBuffer& txInputBytes)
{
    TxInput txInput;
    txInput.UTXOTxHash = txInputBytes.readArray256();
    txInput.UTXOOutputIndex = txInputBytes.readU64();
    txInput.signature = txInputBytes.readArray512();
    return txInput;
}

// ----------------------------------------
// TxOutput
// ----------------------------------------
BytesBuffer serialiseTxOutput(const TxOutput& txOutput)
{
    BytesBuffer serialisedTxOutput;
    serialisedTxOutput.writeU64(txOutput.amount);
    serialisedTxOutput.writeArray256(txOutput.recipient);
    return serialisedTxOutput;
}

TxOutput parseTxOutput(BytesBuffer& txOutputBytes)
{
    TxOutput txOutput;
    txOutput.amount = txOutputBytes.readU64();
    txOutput.recipient = txOutputBytes.readArray256();
    return txOutput;
}

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
    for (const auto& input : tx.txInputs)
    {
        txBytes.writeBytesBuffer(serialiseTxInput(input));
    }

    // Outputs amount
    txBytes.writeU64(tx.txOutputs.size());

    // Outputs
    for (const auto& output : tx.txOutputs)
    {
        txBytes.writeBytesBuffer(serialiseTxOutput(output));
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
        tx.txInputs.push_back(parseTxInput(txBytes));
    }

    // Output amount
    uint64_t outputAmount = txBytes.readU64();
    tx.txOutputs.reserve(outputAmount);

    // Read outputs
    for (uint64_t i = 0; i < outputAmount; i++)
    {
        tx.txOutputs.push_back(parseTxOutput(txBytes));
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
BytesBuffer serialiseBlock(const Block& block)
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

Block parseBlock(BytesBuffer& blockBytes)
{
    Block block;

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
