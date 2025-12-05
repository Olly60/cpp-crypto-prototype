#include "crypto_utils.h"

// ============================================================================
// UTXO DATABASE OPERATIONS
// ============================================================================

namespace {

	std::string makeUtxoKey(const TxInput& txInput) {
		std::string key;
		appendBytes(key, txInput.UTXOTxHash);
		appendBytes(key, txInput.UTXOOutputIndex);
		return key;
	}

	std::string makeUtxoValue(const TxOutput& utxo) {
		std::string value;
		appendBytes(value, utxo.amount);
		appendBytes(value, utxo.recipient);
		return value;
	}

	TxOutput formatUtxoValue(const std::string& value) {
		TxOutput utxo;
		size_t offset = 0;
		std::span<const uint8_t> data(
			reinterpret_cast<const uint8_t*>(value.data()),
			value.size()
		);
		takeBytesInto(utxo.amount, data, offset);
		takeBytesInto(utxo.recipient, data, offset);
		return utxo;
	}

}

void putUtxo(leveldb::DB& db, const TxInput& txInput, const TxOutput& utxo) {
	std::string key = makeUtxoKey(txInput);
	std::string value = makeUtxoValue(utxo);

	leveldb::Status status = db.Put(
		leveldb::WriteOptions(),
		leveldb::Slice(key),
		leveldb::Slice(value)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to put UTXO: " + status.ToString());
	}
}

void deleteUtxo(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);

	leveldb::Status status = db.Delete(
		leveldb::WriteOptions(),
		leveldb::Slice(key)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to delete UTXO: " + status.ToString());
	}
}

TxOutput getUtxo(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	if (!status.ok()) {
		throw std::runtime_error("UTXO not found: " + status.ToString());
	}

	return formatUtxoValue(value);
}

std::unique_ptr<leveldb::DB> openUtxoDb() {
	fs::create_directories(paths::utxo);

	leveldb::Options options;
	options.create_if_missing = true;

	leveldb::DB* raw = nullptr;
	leveldb::Status status = leveldb::DB::Open(
		options,
		(paths::utxo / "leveldb").string(),
		&raw
	);

	if (!status.ok() || !raw) {
		throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
	}

	return std::unique_ptr<leveldb::DB>(raw);
}

bool utxoInDb(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	return status.ok();
}