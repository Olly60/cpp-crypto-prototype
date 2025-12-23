#pragma once
#include "bytes_buffer.h"
#include "crypto_utils.h"
// ----------------------------------------
// TxInput
// ----------------------------------------

BytesBuffer serialiseTxInput(const TxInput& txInput);

TxInput parseTxInput(BytesBuffer& txInputBytes);

// ----------------------------------------
// TxOutput
// ----------------------------------------
BytesBuffer serialiseTxOutput(const TxOutput& txOutput);

TxOutput parseTxOutput(BytesBuffer& txOutputBytes);

// ----------------------------------------
// Tx
// ----------------------------------------
BytesBuffer serialiseTx(const Tx& tx);

Tx parseTx(BytesBuffer& txBytes);

// ----------------------------------------
// BlockHeader
// ----------------------------------------
BytesBuffer serialiseBlockHeader(const BlockHeader& header);

BlockHeader parseBlockHeader(BytesBuffer& headerBytes);

// ----------------------------------------
// Block
// ----------------------------------------
BytesBuffer serialiseBlock(const Block& block);

Block parseBlock(BytesBuffer& blockBytes);