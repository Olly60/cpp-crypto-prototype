#include <sodium.h>
#include "crypto_utils.h"
#include <stdexcept>
#include "block_validation.h"

bool verifyMerkleRoot(const Block& block) {
	return block.header.merkleRoot == getMerkleRoot(block.txs);
}

bool verifyBlockHash(const Block& block, const Array256_t& expectedHash) {
	return getBlockHash(block) == expectedHash;
}

bool validateTx(const Tx& tx) {}

bool validateBlock(Block block) {
	// Calculate block hash
	array256_t blockHash;


	// Verify block header
	// Already in chain
	if (blockChain.count(blockHash) == 1) return false;

	// Previous block not found
	if (blockChain.count(block.header.previousBlockHash) == 0) return false;

	// Timestamp is earlier than previous block
	if (block.header.timestamp < blockChain[block.header.previousBlockHash].header.timestamp) return false;

	// Timestamp is too far in the future
	uint64_t currentTime = time(0);
	if (block.header.timestamp > currentTime + (2 * (60 * 60))) return false;

	// Verify each transaction
	std::vector<UTXOKey> seenUtxo;
	uint64_t blockReward = 0;
	std::vector<uint8_t> txHashes;
	bool isCoinbaseTx = true;
	array256_t txHash;
	std::vector<uint8_t> merkleLeaves;
	for (const Tx& tx : block.txs) {

		hashTransaction(txHash, tx);
		merkleLeaves.insert(merkleLeaves.end(), txHash.begin(), txHash.end());

		// Coinbase transaction
		if (isCoinbaseTx) { isCoinbaseTx = false; continue; }

		// Verify each input
		UTXOKey key;
		uint64_t totalInputAmount = 0;
		uint64_t totalOutputAmount = 0;
		for (const TxInputSigned& txInputSigned : tx.txInputs) {

			key.txHash = txInputSigned.txInput.prevTxHash;
			key.outputIndex = txInputSigned.txInput.outputIndex;

			// UTXO not found
			if (UTXOs.count(key) == 0) return false;

			// Invalid signature
			sha256Of(txHash, &txInputSigned.txInput, sizeof(TxInput));
			if (crypto_sign_verify_detached(txInputSigned.signature.data(), txHash.data(), 32, UTXOs[key].recipient.data()) != 0) return false;

			// Double spending
			if (find(seenUtxo.begin(), seenUtxo.end(), key) != seenUtxo.end()) return false;
			seenUtxo.push_back(key);

			// Calculate total input amount
			totalInputAmount += UTXOs[key].amount;
		}

		// Verify each output
		// Calculate total output amount
		for (UTXO txOutput : tx.txOutputs) {

			// Double spending of transaction
			if (UTXOs.count(key) != 0) return false;
			// Accumulate output amount
			totalOutputAmount += txOutput.amount;
		}
		// Output amount exceeds input amount subtract fee
		uint64_t txFee = std::max(totalInputAmount / 100, (uint64_t)1);
		if (totalOutputAmount > totalInputAmount - txFee) return false;

		// Accumulate total fees
		blockReward += txFee;
	}

	// MerkleRoot invalid
	array256_t merkleRoot;
	sha256Of(merkleRoot, merkleLeaves.data(), merkleLeaves.size());
	if (block.header.merkleRoot != block.header.merkleRoot) return false;

	// Verify coinbase transaction
	{
		// Coinbase transaction should have no inputs
		if (!block.txs[0].txInputs.empty()) return false;

		// Coinbase transaction should have exactly one output
		if (block.txs[0].txOutputs.size() != 1) return false;

		// Coinbase transaction output amount should equal total fees
		uint64_t coinabaseAmount = 0;
		for (const UTXO& coinbaseTxOutput : block.txs[0].txOutputs) {
			coinabaseAmount += coinbaseTxOutput.amount;
		}
		// Coinbase amount exceeds total fees
		if (coinabaseAmount != blockReward) return false;
	}

	// All checks passed
	// Add block to chain
	blockChain[blockHash] = block;

	// Remove used UTXOs
	for (const Tx& tx : block.txs) {
		for (TxInputSigned txInputSigned : tx.txInputs) {
			UTXOKey key;
			key.txHash = txInputSigned.txInput.prevTxHash;
			//key.outputIndex = //txInputSigned.txInput.outputIndex;
			//UTXOs.erase(key);
		}
	}

	// Process transactions
	for (const Tx& tx : block.txs) {
		hashTransaction(txHash, tx);

		// Remove used input UTXO
		for (const TxInputSigned& txInputSigned : tx.txInputs) {
			UTXOKey key;
			key.txHash = txInputSigned.txInput.prevTxHash;
			key.outputIndex = txInputSigned.txInput.outputIndex;
			UTXOs.erase(key);

		}

		// Add new UTXOs from Transactions
		UTXOKey key;
		uint64_t index = 0;
		for (const UTXO& txOutput : tx.txOutputs) {
			key.txHash = txHash;
			key.outputIndex = *reinterpret_cast<uint64_t*>(putUint64Le(index).data());
			UTXOs[key] = txOutput;
			index++;
		}
	}

	// Create UTXOs for Coinbase Transaction outputs
	hashTransaction(txHash, block.txs[0]);
	UTXOKey key;
	uint64_t index = 0;
	for (const UTXO& coinbaseTxOutput : block.txs[0].txOutputs) {
		key.txHash = txHash;
		//key.outputIndex = //*reinterpret_cast<uint64_t*>(putUint64Le(index).data());
		//UTXOs[key] = coinbaseTxOutput;
		index++;
	}

	// Block is valid
	return true;


// Unsupported block version
return false;
}

bool validateBlock(Block block) {
	switch (block.version) {
	case 1: return block_v1::verifyBlock(block);
	default: throw std::runtime_error("Unsupported Block version");
	}
}
