#include <asio.hpp>
#include "crypto_utils.h"
#include "storage.h"
#include <random>
#include "network.h"

// ASIO I/O context
asio::io_context ioContext;

// Connected peers list
std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;

// Hash function for PeerAddress to use in unordered_map
struct PeerAddressHash {
	std::size_t operator()(const PeerAddress& p) const noexcept {
		std::size_t h1 = std::hash<std::string>{}(p.address);
		std::size_t h2 = std::hash<uint16_t>{}(p.port);
		return h1 ^ (h2 << 1); // combine the hashes
	}
};

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

struct Handshake {
	uint32_t protocolVersion{ 1 };
	uint32_t networkId{ 1 };
	uint64_t services{ SERVICE_FULL_NODE };
	uint64_t nonce{ 0 };
	Array256_t blockchainTip{};
};

struct Inv {
	std::vector<Array256_t> blockHashes;
};



void incomingHandshakeAsync(std::shared_ptr<asio::ip::tcp::socket> socket) {
	// Step 1: read peer handshake
	auto buffer = std::make_unique<std::vector<uint8_t>>(
		sizeof(Handshake::protocolVersion) + sizeof(Handshake::networkId) +
		sizeof(Handshake::services) + sizeof(Handshake::nonce) +
		sizeof(Handshake::blockchainTip)
	);

	asio::async_read(*socket, asio::buffer(*buffer),
		[socket, buffer = std::move(buffer)](std::error_code ec, std::size_t) mutable {
			if (ec) {
				socket->close();
				return;
			}

			// Deserialize peer handshake
			Handshake theirHandshake;
			size_t offset = 0;
			takeBytesInto(theirHandshake.protocolVersion, *buffer, offset);
			takeBytesInto(theirHandshake.networkId, *buffer, offset);
			takeBytesInto(theirHandshake.services, *buffer, offset);
			takeBytesInto(theirHandshake.nonce, *buffer, offset);
			takeBytesInto(theirHandshake.blockchainTip, *buffer, offset);

			// Validate peer handshake
			if (theirHandshake.protocolVersion != currentProtocolVersion ||
				theirHandshake.networkId != currentNetworkId ||
				theirHandshake.nonce == localNonce) {
				socket->close();
				return;
			}

			// Step 2: prepare our handshake response
			auto outBuffer = std::make_unique<std::vector<uint8_t>>();
			Handshake myHandshake;
			myHandshake.protocolVersion = currentProtocolVersion;
			myHandshake.networkId = currentNetworkId;
			myHandshake.services = 0b1;
			myHandshake.nonce = localNonce;
			myHandshake.blockchainTip = getBlockchainTip();

			appendBytes(*outBuffer, myHandshake.protocolVersion);
			appendBytes(*outBuffer, myHandshake.networkId);
			appendBytes(*outBuffer, myHandshake.services);
			appendBytes(*outBuffer, myHandshake.nonce);
			appendBytes(*outBuffer, myHandshake.blockchainTip);

			asio::async_write(*socket, asio::buffer(*outBuffer),
				[socket, theirHandshake, outBuffer = std::move(outBuffer), myHandshake](std::error_code ec, std::size_t) mutable {
					if (ec) {
						socket->close();
						return;
					}

					// Step 3: read peer verack
					auto verackBuffer = std::make_unique<uint8_t>();
					asio::async_read(*socket, asio::buffer(verackBuffer.get(), 1),
						[socket, theirHandshake, verackBuffer = std::move(verackBuffer)](std::error_code ec, std::size_t) mutable {
							if (ec || *verackBuffer != 0x01) {
								socket->close();
								return;
							}

							// Step 4: send our verack
							auto myVerack = std::make_unique<uint8_t>(0x01);
							asio::async_write(*socket, asio::buffer(myVerack.get(), 1),
								[socket, theirHandshake, myVerack = std::move(myVerack)](std::error_code ec, std::size_t) mutable {
									if (ec) {
										socket->close();
										return;
									}

									// Handshake complete, add peer
									PeerAddress newPeerAddr;
									newPeerAddr.address = socket->remote_endpoint().address().to_string();
									newPeerAddr.port = socket->remote_endpoint().port();

									PeerStatus newPeerStatus;
									newPeerStatus.services = theirHandshake.services;
									newPeerStatus.lastSeen = getCurrentTimestamp();

									peers.insert({ newPeerAddr, newPeerStatus });
								});
						});
				});
		});
}

void outgoingHandshakeAsync(std::shared_ptr<asio::ip::tcp::socket> socket) {
	// Step 0: prepare handshake data
	auto myHandshake = std::make_unique<Handshake>();
	myHandshake->protocolVersion = currentProtocolVersion;
	myHandshake->networkId = currentNetworkId;
	myHandshake->services = SERVICE_FULL_NODE;
	myHandshake->nonce = localNonce;
	myHandshake->blockchainTip = getBlockchainTip();

	// Step 1: prepare outgoing buffer
	auto outBuffer = std::make_unique<std::vector<uint8_t>>();
	appendBytes(*outBuffer, myHandshake->protocolVersion);
	appendBytes(*outBuffer, myHandshake->networkId);
	appendBytes(*outBuffer, myHandshake->services);
	appendBytes(*outBuffer, myHandshake->nonce);
	appendBytes(*outBuffer, myHandshake->blockchainTip);

	// Step 2: async write handshake
	asio::async_write(*socket, asio::buffer(*outBuffer),
		[socket, myHandshake = std::move(myHandshake), outBuffer = std::move(outBuffer)]
		(std::error_code ec, std::size_t) mutable {
			if (ec) {
				socket->close();
				return;
			}

			// Step 3: read peer handshake
			auto theirBuffer = std::make_unique<std::vector<uint8_t>>(
				sizeof(Handshake::protocolVersion) +
				sizeof(Handshake::networkId) +
				sizeof(Handshake::services) +
				sizeof(Handshake::nonce) +
				sizeof(Handshake::blockchainTip)
			);

			asio::async_read(*socket, asio::buffer(*theirBuffer),
				[socket, myHandshake = std::move(myHandshake), theirBuffer = std::move(theirBuffer)]
				(std::error_code ec, std::size_t) mutable {
					if (ec) {
						socket->close();
						return;
					}

					Handshake theirHandshake;
					size_t offset = 0;
					takeBytesInto(theirHandshake.protocolVersion, *theirBuffer, offset);
					takeBytesInto(theirHandshake.networkId, *theirBuffer, offset);
					takeBytesInto(theirHandshake.services, *theirBuffer, offset);
					takeBytesInto(theirHandshake.nonce, *theirBuffer, offset);
					takeBytesInto(theirHandshake.blockchainTip, *theirBuffer, offset);

					// Validate peer
					if (theirHandshake.protocolVersion != currentProtocolVersion ||
						theirHandshake.networkId != currentNetworkId ||
						theirHandshake.nonce == localNonce) {
						socket->close();
						return;
					}

					// Step 4: send verack
					auto verack = std::make_unique<uint8_t>(0x01);
					asio::async_write(*socket, asio::buffer(verack.get(), 1),
						[socket, theirHandshake, verack = std::move(verack)]
						(std::error_code ec, std::size_t) mutable {
							if (ec) {
								socket->close();
								return;
							}

							// Step 5: read peer verack
							auto theirVerack = std::make_unique<uint8_t>();
							asio::async_read(*socket, asio::buffer(theirVerack.get(), 1),
								[socket, theirHandshake, theirVerack = std::move(theirVerack)]
								(std::error_code ec, std::size_t) mutable {
									if (ec || *theirVerack != 0x01) {
										socket->close();
										return;
									}

									// Handshake complete! Add peer
									PeerAddress newPeerAddr;
									newPeerAddr.address = socket->remote_endpoint().address().to_string();
									newPeerAddr.port = socket->remote_endpoint().port();

									PeerStatus newPeerStatus;
									newPeerStatus.services = theirHandshake.services;
									newPeerStatus.lastSeen = getCurrentTimestamp();

									peers.insert({ newPeerAddr, newPeerStatus });

								});
						});
				});
		});
}

Array256_t requestBlockHashList(std::shared_ptr<asio::ip::tcp::socket> socket,) {
	return Array256_t{};
}

Array256_t findCommonAncestor(std::shared_ptr<asio::ip::tcp::socket> socket) {

	return Array256_t{};
}

void startAccepting(asio::ip::tcp::acceptor& acceptor) {
	acceptor.async_accept([&acceptor](std::error_code ec, asio::ip::tcp::socket socket) {
		if (!ec) {
			
			PeerAddress addr;
			addr.address = socket.remote_endpoint().address().to_string();
			addr.port = socket.remote_endpoint().port();

			if (peers.find(addr) != peers.end()) {
				// Peer exists, just update lastSeen
				peers[addr].lastSeen = getCurrentTimestamp();
			}
			else {
				// Peer is new, start async handshake
				incomingHandshakeAsync(std::make_shared<asio::ip::tcp::socket>(socket));
				// The handshake itself will add the peer once complete
			}
		}
		startAccepting(acceptor); // schedule next accept
		});
}

int main() {
	asio::io_context ioContext;
	asio::ip::tcp::acceptor acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

	// Start accepting connections
	startAccepting(acceptor);

	// Run the event loop
	ioContext.run();
}
