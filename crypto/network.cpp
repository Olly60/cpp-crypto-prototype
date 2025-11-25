#include <asio.hpp>
#include "crypto_utils.h"
#include "storage.h"
#include <random>
#include "network.h"

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

void incomingConnection(asio::ip::tcp::socket socket) {
	HandShake packet;
	std::vector<uint8_t> buffer(sizeof(packet.protocolVersion) + sizeof(packet.networkId) + sizeof(packet.services) + sizeof(packet.nonce) + sizeof(packet.blockchainTip));
	asio::read(socket, asio::buffer(buffer.data(), buffer.size()));
	size_t offset = 0;
	takeBytesInto(packet.protocolVersion, buffer, offset);
	takeBytesInto(packet.networkId, buffer, offset);
	takeBytesInto(packet.services, buffer, offset);
	takeBytesInto(packet.nonce, buffer, offset);
	takeBytesInto(packet.blockchainTip, buffer, offset);
	if (packet.protocolVersion != currentProtocolVersion || packet.networkId != currentNetworkId || packet.nonce == localNonce) {
		socket.close();
		return;
	}

	HandShake response;
	response.protocolVersion = currentProtocolVersion;
	response.networkId = currentNetworkId;
	response.services = 0b1;
	response.nonce = localNonce;
	response.blockchainTip = getBlockchainTip();

	std::vector<uint8_t> out;
	appendBytes(out, response.protocolVersion);
	appendBytes(out, response.networkId);
	appendBytes(out, response.services);
	appendBytes(out, response.nonce);
	appendBytes(out, response.blockchainTip);

	asio::write(socket, asio::buffer(out));

	// Step 4: read their verack (1 byte)
	uint8_t theirVerack;
	asio::read(socket, asio::buffer(&theirVerack, 1));

	uint8_t verack = 0x01;
	if (theirVerack != verack) {
		socket.close();
		return;
	}

	// Step 5: send our verack
	uint8_t myVerack = verack;
	asio::write(socket, asio::buffer(&myVerack, 1));

}

void outgoingConnection(const std::string& peerAddress, uint16_t peerPort) {
	try {
		asio::ip::tcp::socket socket(ioContext);
		socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address(peerAddress), peerPort));

		// Step 1: send handshake
		HandShake hs;
		hs.protocolVersion = currentProtocolVersion;
		hs.networkId = currentNetworkId;
		hs.services = SERVICE_FULL_NODE;
		hs.nonce = localNonce;
		hs.blockchainTip = getBlockchainTip();

		std::vector<uint8_t> out;
		appendBytes(out, hs.protocolVersion);
		appendBytes(out, hs.networkId);
		appendBytes(out, hs.services);
		appendBytes(out, hs.nonce);
		appendBytes(out, hs.blockchainTip);

		asio::write(socket, asio::buffer(out));

		// Step 2: read peer handshake
		HandShake peerHs;
		std::vector<uint8_t> buffer(sizeof(peerHs.protocolVersion) + sizeof(peerHs.networkId) +
			sizeof(peerHs.services) + sizeof(peerHs.nonce) + sizeof(peerHs.blockchainTip));
		asio::read(socket, asio::buffer(buffer.data(), buffer.size()));

		size_t offset = 0;
		takeBytesInto(peerHs.protocolVersion, buffer, offset);
		takeBytesInto(peerHs.networkId, buffer, offset);
		takeBytesInto(peerHs.services, buffer, offset);
		takeBytesInto(peerHs.nonce, buffer, offset);
		takeBytesInto(peerHs.blockchainTip, buffer, offset);

		// Validate peer
		if (peerHs.protocolVersion != currentProtocolVersion ||
			peerHs.networkId != currentNetworkId ||
			peerHs.nonce == localNonce) {
			socket.close();
			return;
		}

		// Step 3: send verack
		uint8_t verack = 0x01;
		asio::write(socket, asio::buffer(&verack, 1));

		// Step 4: read peer verack
		uint8_t theirVerack;
		asio::read(socket, asio::buffer(&theirVerack, 1));
		if (theirVerack != verack) {
			socket.close();
			return;
		}

		// Handshake complete!
		// Add peer to in-memory peers list
		Peer newPeer;
		newPeer.address = peerAddress;
		newPeer.port = peerPort;
		newPeer.services = peerHs.services;
		newPeer.blockchainTip = peerHs.blockchainTip;
		connectedPeers.push_back(newPeer);

		// Optionally store peer on disk
		storePeerAddress(peerAddress, peerPort);

		// Step 5: Compare blockchain tips
		auto localTip = getBlockchainTip();
		if (peerHs.blockchainTip != localTip) {
			// TODO: request missing blocks using getBlocks/getData
		}

		// From here, start your async read loop to receive blocks/txs from this peer
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to connect to " << peerAddress << ":" << peerPort
			<< " - " << e.what() << "\n";
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