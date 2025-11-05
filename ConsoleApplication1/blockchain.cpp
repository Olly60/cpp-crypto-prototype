#include <sodium.h>
#include <unordered_map>
#include "types.h"
#include "utils.h"
#include <vector>
#include <time.h>
#include <fstream>

static bool processBlock(const Block& block, std::unordered_map<hash256_t, Block>& blockChain, std::unordered_map<UTXOKey, UTXO, UTXOKeyHash>& UTXOs) {
	// Version 1 block verification
	if (block.header.version == 1) {

		// Calculate block hash
		hash256_t blockHash;
		std::array<uint8_t, 96> serializedHeader;
		memcpy(serializedHeader.data(), putUint64Le(block.header.version).data(), 8);
		memcpy(serializedHeader.data() + 8, block.header.previousBlockHash.data(), 32);
		memcpy(serializedHeader.data() + 40, block.header.merkleRoot.data(), 32);
		memcpy(serializedHeader.data() + 72, putUint64Le(block.header.timestamp).data(), 8);
		memcpy(serializedHeader.data() + 80, putUint64Le(block.header.difficulty).data(), 8);
		memcpy(serializedHeader.data() + 88, putUint64Le(block.header.nonce).data(), 8);
		sha256Of(blockHash, serializedHeader.data(), serializedHeader.size());

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
		hash256_t txHash;
		std::vector<uint8_t> merkleLeaves;
		for (const Transaction& tx : block.transactions) {

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
		hash256_t merkleRoot;
		sha256Of(merkleRoot, merkleLeaves.data(), merkleLeaves.size());
		if (block.header.merkleRoot != block.header.merkleRoot) return false;

		// Verify coinbase transaction
		// Coinbase transaction should have no inputs
		if (!block.transactions[0].txInputs.empty()) return false;

		// Coinbase transaction should have exactly one output
		if (block.transactions[0].txOutputs.size() != 1) return false;

		// Coinbase transaction output amount should equal total fees
		uint64_t coinabaseAmount = 0;
		for (const UTXO& coinbaseTxOutput : block.transactions[0].txOutputs) {
			coinabaseAmount += coinbaseTxOutput.amount;
		}
		// Coinbase amount exceeds total fees
		if (coinabaseAmount != blockReward) return false;

		// All checks passed
		// Add block to chain
		blockChain[blockHash] = block;

		// Remove used UTXOs
		for (const Transaction& tx : block.transactions) {
			for (TxInputSigned txInputSigned : tx.txInputs) {
				UTXOKey key;
				key.txHash = txInputSigned.txInput.prevTxHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;
				UTXOs.erase(key);
			}
		}


		for (const Transaction& tx : block.transactions) {
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
		hashTransaction(txHash, block.transactions[0]);
		UTXOKey key;
		uint64_t index = 0;
		for (const UTXO& coinbaseTxOutput : block.transactions[0].txOutputs) {
			key.txHash = txHash;
			key.outputIndex = *reinterpret_cast<uint64_t*>(putUint64Le(index).data());
			UTXOs[key] = coinbaseTxOutput;
			index++;
		}

		// Block is valid
		return true;
	}

	// Unsupported block version
	return false;
}