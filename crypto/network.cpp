#include <asio.hpp>
#include "crypto_utils.h"
#include "storage.h"
#include <random>
#include "network.h"



// ASIO I/O context
asio::io_context ioContext;

// Connected peers list
std::unordered_map<PeerAddress, PeerStatus> peers;

// Services
constexpr uint64_t SERVICE_FULL_NODE = 0b00000001; // bit 0

// Protocols
constexpr uint32_t currentProtocolVersion = 1;
constexpr uint32_t currentNetworkId = 1;

uint64_t generateNonce() {
	std::random_device rd;
	uint64_t v = 0;
	for (int i = 0; i < 8; i++) {
		v = (v << 8) | (rd() & 0xFF);
	}
	return v;
}

static uint64_t localNonce = generateNonce();

struct HandShake {
	uint32_t protocolVersion{ 1 };
	uint32_t networkId{ 1 };
	uint64_t services{ SERVICE_FULL_NODE };
	uint64_t nonce{ 0 };
	Array256_t blockchainTip{};
};

struct Inv {
	std::vector<Array256_t> blockHashes;
};

bool incomingHandShake(asio::ip::tcp::socket socket) {
	try {
		// Step 1: read their handshake
		HandShake theirHandShake;
		std::vector<uint8_t> buffer(sizeof(theirHandShake.protocolVersion) + sizeof(theirHandShake.networkId) + sizeof(theirHandShake.services) + sizeof(theirHandShake.nonce) + sizeof(theirHandShake.blockchainTip));
		asio::read(socket, asio::buffer(buffer.data(), buffer.size()));
		size_t offset = 0;
		takeBytesInto(theirHandShake.protocolVersion, buffer, offset);
		takeBytesInto(theirHandShake.networkId, buffer, offset);
		takeBytesInto(theirHandShake.services, buffer, offset);
		takeBytesInto(theirHandShake.nonce, buffer, offset);
		takeBytesInto(theirHandShake.blockchainTip, buffer, offset);
		if (theirHandShake.protocolVersion != currentProtocolVersion || theirHandShake.networkId != currentNetworkId || theirHandShake.nonce == localNonce) {
			socket.close();
			return false;
		}

		// Step 2: send our handshake
		HandShake myHandShake;
		myHandShake.protocolVersion = currentProtocolVersion;
		myHandShake.networkId = currentNetworkId;
		myHandShake.services = 0b1;
		myHandShake.nonce = localNonce;
		myHandShake.blockchainTip = getBlockchainTip();

		std::vector<uint8_t> out;
		appendBytes(out, myHandShake.protocolVersion);
		appendBytes(out, myHandShake.networkId);
		appendBytes(out, myHandShake.services);
		appendBytes(out, myHandShake.nonce);
		appendBytes(out, myHandShake.blockchainTip);

		asio::write(socket, asio::buffer(out));

		// Step 3: read their verack
		uint8_t theirVerack;
		asio::read(socket, asio::buffer(&theirVerack, 1));

		uint8_t verack = 0x01;
		if (theirVerack != verack) {
			socket.close();
			return false;
		}

		// Step 4: send our verack
		uint8_t myVerack = verack;
		asio::write(socket, asio::buffer(&myVerack, 1));

		// Handshake complete!
		// Add peer to in-memory peers list
		PeerAddress newPeerAddr;
		newPeerAddr.address = socket.remote_endpoint().address().to_string();
		newPeerAddr.port = socket.remote_endpoint().port();
		PeerStatus newPeerStatus;
		newPeerStatus.services = theirHandShake.services;
		newPeerStatus.lastSeen = getCurrentTimestamp();
		peers.insert({ newPeerAddr, newPeerStatus });
	}
	catch (const std::exception& e) {
		return false;
	}
}

bool outgoingHandShake(const std::string& peerAddress, uint16_t peerPort) {
	try {
		asio::ip::tcp::socket socket(ioContext);
		socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address(peerAddress), peerPort));

		// Step 1: send handshake
		HandShake myHandShake;
		myHandShake.protocolVersion = currentProtocolVersion;
		myHandShake.networkId = currentNetworkId;
		myHandShake.services = SERVICE_FULL_NODE;
		myHandShake.nonce = localNonce;
		myHandShake.blockchainTip = getBlockchainTip();

		std::vector<uint8_t> out;
		appendBytes(out, myHandShake.protocolVersion);
		appendBytes(out, myHandShake.networkId);
		appendBytes(out, myHandShake.services);
		appendBytes(out, myHandShake.nonce);
		appendBytes(out, myHandShake.blockchainTip);

		asio::write(socket, asio::buffer(out));

		// Step 2: read peer handshake
		HandShake theirHandShake;
		std::vector<uint8_t> buffer(sizeof(theirHandShake.protocolVersion) + sizeof(theirHandShake.networkId) +
			sizeof(theirHandShake.services) + sizeof(theirHandShake.nonce) + sizeof(theirHandShake.blockchainTip));
		asio::read(socket, asio::buffer(buffer.data(), buffer.size()));

		size_t offset = 0;
		takeBytesInto(theirHandShake.protocolVersion, buffer, offset);
		takeBytesInto(theirHandShake.networkId, buffer, offset);
		takeBytesInto(theirHandShake.services, buffer, offset);
		takeBytesInto(theirHandShake.nonce, buffer, offset);
		takeBytesInto(theirHandShake.blockchainTip, buffer, offset);

		// Validate peer
		if (theirHandShake.protocolVersion != currentProtocolVersion ||
			theirHandShake.networkId != currentNetworkId ||
			theirHandShake.nonce == localNonce) {
			socket.close();
			return false;
		}

		// Step 3: send verack
		uint8_t verack = 0x01;
		asio::write(socket, asio::buffer(&verack, 1));

		// Step 4: read peer verack
		uint8_t theirVerack;
		asio::read(socket, asio::buffer(&theirVerack, 1));
		if (theirVerack != verack) {
			socket.close();
			return false;
		}

		// Handshake complete!
		// Add peer to in-memory peers list
		PeerAddress newPeerAddr;
		newPeerAddr.address = socket.remote_endpoint().address().to_string();
		newPeerAddr.port = socket.remote_endpoint().port();
		PeerStatus newPeerStatus;
		newPeerStatus.services = theirHandShake.services;
		newPeerStatus.lastSeen = getCurrentTimestamp();
		peers.insert({ newPeerAddr, newPeerStatus });

		// Step 5: Compare blockchain tips
		auto localTip = getBlockchainTip();
		if (theirHandShake.blockchainTip != localTip) {
			// TODO: request missing blocks using getBlocks/getData
		}

		// From here, start your async read loop to receive blocks/txs from this peer
	}
	catch (const std::exception& e) {
		return false;
	}
}

void sendBlock(asio::ip::tcp::socket& socket, const Block& block) {
	// Serialize the block
	std::vector<uint8_t> blockBytes = serialiseBlock(block);
	// Prepare the size prefix
	uint32_t blockSize = static_cast<uint32_t>(blockBytes.size());
	std::array<uint8_t, 4> sizeBytes = serialiseNumberLe(blockSize);
	// Send the size prefix
	asio::write(socket, asio::buffer(sizeBytes));
	// Send the block data
	asio::write(socket, asio::buffer(blockBytes));
}

void sendTxToMempool(asio::ip::tcp::socket& socket, const Tx& tx) {

	// Serialize the transaction
	std::vector<uint8_t> txBytes = serialiseTx(tx);
	// Prepare the size prefix
	uint32_t txSize = static_cast<uint32_t>(txBytes.size());
	std::array<uint8_t, 4> sizeBytes = serialiseNumberLe(txSize);
	// Send the size prefix
	asio::write(socket, asio::buffer(sizeBytes));
	// Send the transaction data
	asio::write(socket, asio::buffer(txBytes));
}

void networkLoop() {
	asio::ip::tcp::socket socket(ioContext);
	acceptor.accept(socket);  // new incoming connection

	PeerAddress addr;
	addr.address = socket.remote_endpoint().address().to_string();
	addr.port = socket.remote_endpoint().port();

	// Check if peer exists
	if (peers.find(addr) != peers.end()) {
		// Peer already known, you can skip handshake or just update lastSeen
		peers[addr].lastSeen = getCurrentTimestamp();
	}
	else {
		// Peer is new, perform handshake
		if (incomingHandShake(std::move(socket))) {
			// handshake succeeded, add peer
			PeerStatus status;
			status.services = 0b1; // example
			status.lastSeen = getCurrentTimestamp();
			peers.insert({ addr, status });
		}
	}
}