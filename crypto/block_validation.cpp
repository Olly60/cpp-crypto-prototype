#include <sodium.h>
#include "crypto_utils.h"
#include <stdexcept>
#include "block_validation.h"
#include "storage.h"
#include <set>

bool verifyMerkleRoot(const Block& block) {
	return block.header.merkleRoot == getMerkleRoot(block.txs);
}

bool verifyBlockHash(const Block& block, const Array256_t& expectedHash) {
	return getBlockHash(block) == expectedHash;
}

bool verifyTx(const Tx& tx) {

	// Signature
	verifyTxSignature(tx);

	auto txHash = getTxHash(tx);

	// Verify each input
	uint64_t totalInputAmount = 0;
	uint64_t totalOutputAmount = 0;
	auto utxoDb = openUtxoDb();
	std::set<std::pair<Array256_t, uint64_t>> seenUTXOs;
	for (const TxInput& txInput : tx.txInputs) {

		// UTXO not found
		if (utxoInDb(*utxoDb, txInput)) return false;

		auto key = std::make_pair(txInput.UTXOTxHash, txInput.UTXOOutputIndex);
		if (!seenUTXOs.insert(key).second) return false;

		// Calculate total input amount
		totalInputAmount += getUtxoValue(*utxoDb, txInput).amount;
	}

	// Verify each output
	// Calculate total output amount
	for (TxOutput txOutput : tx.txOutputs) {

		// Accumulate output amount
		totalOutputAmount += txOutput.amount;
	}
	// Output amount exceeds input amount subtract fee
	uint64_t minFee = 1;
	uint64_t txFee = std::max(totalInputAmount / 100, minFee);
	if (totalOutputAmount > totalInputAmount - txFee) return false;
}

bool verifyBlock(Block block) {
	// Calculate block hash
	Array256_t blockHash = getBlockHash(block);

	// --------------------------------------------
	// Verify Block Header
	// --------------------------------------------

	// Already in chain
	if (blockExists(blockHash)) return false;

	// Previous block not found
	if (!blockExists(block.header.prevBlockHash)) return false;

	BlockHeader prevBlock = getBlockHeader(block.header.prevBlockHash);

	// Timestamp is earlier than previous block
	if (block.header.timestamp < prevBlock.timestamp) return false;

	// Timestamp is too far in the future
	if (block.header.timestamp > getCurrentTimestamp() + (2 * (60 * 60))) return false;

	// workout difficulty

	// height

	// --------------------------------------------
	// Verify Transactions
	// --------------------------------------------

	std::set<std::pair<Array256_t, uint64_t>> seenUTXOs;
	bool isCoinBase = true;

	for (const Tx& tx : block.txs) {
		// Skip coinbase transaction
		if (isCoinBase) {
			isCoinBase = false;
			continue;
		}
		// Verify each individual transaction
		if (verifyTx(tx)) return false;

		// Check Transactions all have unique inputs
		for (const auto& in : tx.txInputs) {
			auto key = std::make_pair(in.UTXOTxHash, in.UTXOOutputIndex);
			if (!seenUTXOs.insert(key).second) return false;
		}
	}


	// MerkleRoot invalid
	if (block.header.merkleRoot != getMerkleRoot(block.txs)) return false;

	// Verify coinbase transaction
	{
		// Coinbase transaction should have no inputs
		if (!block.txs[0].txInputs.empty()) return false;


		// Coinbase transaction output amount should equal total fees
		uint64_t coinabaseAmount = 0;
		for (const UTXO& coinbaseTxOutput : block.txs[0].txOutputs) {
			coinabaseAmount += coinbaseTxOutput.amount;
		}
		// Coinbase amount exceeds total fees
		if (coinabaseAmount != blockReward) return false;
	}

	// Block is valid
	return true;

}
