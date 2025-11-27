#include <sodium.h>
#include "crypto_utils.h"
#include <stdexcept>
#include "block_verification.h"
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
	uint64_t txFee = std::max(totalInputAmount / 100, uint64_t(1));
	if (totalOutputAmount > totalInputAmount - txFee) return false;
}

bool verifyBlock(Block block) {
	// Calculate block hash
	Array256_t blockHash = getBlockHash(block);

	// --------------------------------------------
	// Verify Block Header
	// --------------------------------------------

	// version
	if (block.header.version != 1) return false;
	// Already in chain
	if (blockExists(blockHash)) return false;

	// Previous block not found
	if (!blockExists(block.header.prevBlockHash)) return false;

	BlockHeader prevBlock = getBlockHeader(block.header.prevBlockHash);

	// MerkleRoot invalid
	if (block.header.merkleRoot != getMerkleRoot(block.txs)) return false;

	// Timestamp is earlier than previous block
	if (block.header.timestamp < prevBlock.timestamp) return false;

	// Timestamp is too far in the future
	if (block.header.timestamp > getCurrentTimestamp() + (60 * 10)) return false;

	// workout difficulty


	// --------------------------------------------
	// Verify Transactions
	// --------------------------------------------

	std::set<std::pair<Array256_t, uint64_t>> seenUTXOs;
	bool isCoinBase = true;

	uint64_t blockFee = 0;
	auto utxoDb = openUtxoDb();

	for (const Tx& tx : block.txs) {
		// Skip coinbase transaction
		if (isCoinBase) {
			isCoinBase = false;
			continue;
		}
		// Verify each individual transaction
		if (verifyTx(tx)) return false;

		uint64_t totalInputAmount = 0;
		for (const auto& txInput : tx.txInputs) {

			// Get the amount being spent in the transaction
			totalInputAmount += getUtxoValue(*utxoDb, txInput).amount;

			// Check Transactions all have unique inputs
			auto key = std::make_pair(txInput.UTXOTxHash, txInput.UTXOOutputIndex);
			if (!seenUTXOs.insert(key).second) return false;
		}
		blockFee += std::max(totalInputAmount / 100, uint64_t(1));
	}



	// Verify coinbase transaction
	const Tx& coinbaseTx = block.txs[0];

	if (!coinbaseTx.txInputs.empty()) return false; // Coinbase has no inputs

	uint64_t coinbaseAmount = 0;
	for (const TxOutput& out : coinbaseTx.txOutputs) coinbaseAmount += out.amount;

	if (coinbaseAmount != blockFee) return false;

	return true;

}
