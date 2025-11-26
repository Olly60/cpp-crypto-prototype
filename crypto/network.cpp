#include <asio.hpp>
#include "crypto_utils.h"
#include "storage.h"
#include <random>
#include "network.h"
#include "block_validation.h"
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

// Handshake structure
struct Handshake {
	uint32_t protocolVersion = myProtocolVersion;
	Array256_t genesisBlockHash = myGenesisBlockHash;
	uint64_t services = SERVICE_FULL_NODE;
	uint64_t nonce = localNonce;
	Array256_t blockchainTip = getBlockchainTip();
};

std::vector<Tx> mempool; // In-memory mempool of transactions

// Services
constexpr uint64_t SERVICE_FULL_NODE = 0b1;

static constexpr uint32_t myProtocolVersion = 1;
static const Array256_t myGenesisBlockHash = getGenesisBlockHash();

static uint64_t localNonce = []() -> uint64_t {
	std::random_device rd;
	std::mt19937_64 gen(rd());                 // 64-bit PRNG
	std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
	return dist(gen);
	}();

// Protocols
enum class ProtocolMessage : uint8_t {
	Handshake = 1,           // Version, features, best height, etc.
	Ping = 2,
	GetHeader = 3,          // Contains block locator list
	GetBlock = 4,            // Request full block by hash
	BroadcastBlock = 5,     // Optional: async broadcast of new block
	BroadcastTransaction = 6, // Optional: async broadcast of new tx
	GetMempool = 7,         // Optional: request list of tx hashes
};

void syncWithPeer(std::shared_ptr<asio::ip::tcp::socket> socket) {

	// Step 1: request peer's blockchain hashes
	requestBlockHashList(socket, [socket](std::error_code ec, std::vector<Array256_t> blockHashes) mutable {
		if (ec) {
			socket->close();
			return;
		}
		// Step 2: compare work

		});
}

// ===========================================================
// Handshake procedures
// ===========================================================

void respondHandshake(std::shared_ptr<asio::ip::tcp::socket> socket) {
	// Step 1: read peer handshake
	auto buffer = std::make_unique<std::array<uint8_t,
		sizeof(Handshake::protocolVersion) +
		sizeof(Handshake::genesisBlockHash) +
		sizeof(Handshake::services) +
		sizeof(Handshake::nonce) +
		sizeof(Handshake::blockchainTip)>>();

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
			takeBytesInto(theirHandshake.genesisBlockHash, *buffer, offset);
			takeBytesInto(theirHandshake.services, *buffer, offset);
			takeBytesInto(theirHandshake.nonce, *buffer, offset);
			takeBytesInto(theirHandshake.blockchainTip, *buffer, offset);

			// Validate peer handshake
			if (theirHandshake.protocolVersion != myProtocolVersion ||
				theirHandshake.genesisBlockHash != myGenesisBlockHash ||
				theirHandshake.nonce == localNonce) {
				socket->close();
				return;
			}

			// Step 2: prepare our handshake response
			auto outBuffer = std::make_unique<std::vector<uint8_t>>();
			Handshake myHandshake;

			appendBytes(*outBuffer, myHandshake.protocolVersion);
			appendBytes(*outBuffer, myHandshake.genesisBlockHash);
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

void requestHandshake(std::shared_ptr<asio::ip::tcp::socket> socket) {
	auto myHandshake = std::make_unique<Handshake>();

	// Step 1: prepare outgoing buffer
	auto outBuffer = std::make_unique<std::vector<uint8_t>>();
	appendBytes(*outBuffer, myHandshake->protocolVersion);
	appendBytes(*outBuffer, myHandshake->genesisBlockHash);
	appendBytes(*outBuffer, myHandshake->services);
	appendBytes(*outBuffer, myHandshake->nonce);
	appendBytes(*outBuffer, myHandshake->blockchainTip);

	auto messageType = std::make_unique<uint8_t>(ProtocolMessage::Handshake);
	asio::async_write(*socket, asio::buffer(messageType.get(), 1), [socket, myHandshake = std::move(myHandshake), outBuffer = std::move(outBuffer)]
	(std::error_code ec, std::size_t) mutable {
			// Step 2: async write handshake
			asio::async_write(*socket, asio::buffer(*outBuffer),
				[socket, myHandshake = std::move(myHandshake), outBuffer = std::move(outBuffer)]
				(std::error_code ec, std::size_t) mutable {
					if (ec) {
						socket->close();
						return;
					}

					// Step 3: read peer handshake
					auto theirBuffer = std::make_unique<std::array<uint8_t,
						sizeof(Handshake::protocolVersion) +
						sizeof(Handshake::genesisBlockHash) +
						sizeof(Handshake::services) +
						sizeof(Handshake::nonce) +
						sizeof(Handshake::blockchainTip)>>();

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
							takeBytesInto(theirHandshake.genesisBlockHash, *theirBuffer, offset);
							takeBytesInto(theirHandshake.services, *theirBuffer, offset);
							takeBytesInto(theirHandshake.nonce, *theirBuffer, offset);
							takeBytesInto(theirHandshake.blockchainTip, *theirBuffer, offset);

							// Validate peer
							if (theirHandshake.protocolVersion != myProtocolVersion ||
								theirHandshake.genesisBlockHash != myGenesisBlockHash ||
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
}

// ============================================
// Service functions
// ============================================

void requestPing(std::shared_ptr<asio::ip::tcp::socket> socket)
{
	// Step 1: send ping message
	auto pingMsg = std::make_shared<uint8_t>(ProtocolMessage::Ping);
	asio::async_write(*socket, asio::buffer(pingMsg.get(), 1),
		[socket, pingMsg](std::error_code ec, std::size_t) {
			if (ec) {
				return;
			}
			// Step 2: read pong response
			auto theirResponse = std::make_shared<uint8_t>(1);
			asio::async_read(*socket, asio::buffer(theirResponse.get(), 1),
				[socket, theirResponse](std::error_code ec, std::size_t) {
					if (ec) {
						return;
					}
					if (*theirResponse == 1) {
						PeerAddress addr;
						addr.address = socket->remote_endpoint().address().to_string();
						addr.port = socket->remote_endpoint().port();
						peers[addr].lastSeen = getCurrentTimestamp();
					}
					else {
						return;
					}
				});
		});
}

void requestBlockHeader(std::shared_ptr<asio::ip::tcp::socket> socket, Array256_t blockHash, std::function<void(std::error_code, BlockHeader)> onDone)
{
	// Step 1: send request message type
	auto requestMsgType = std::make_shared<uint8_t>(ProtocolMessage::GetHeader);
	asio::async_write(*socket, asio::buffer(requestMsgType.get(), 1),
		[socket, blockHash, onDone](std::error_code ec, std::size_t) {
			if (ec) {
				onDone(ec, {});
				return;
			}
			// Step 2: send block hash
			auto hashBuf = std::make_shared<Array256_t>(blockHash);
			asio::async_write(*socket, asio::buffer(*hashBuf),
				[socket, onDone](std::error_code ec, std::size_t) {
					if (ec) {
						onDone(ec, {});
						return;
					}
					// Step 3: read block header size
					auto sizeBuf = std::make_shared<std::array<uint8_t, 8>>();
					asio::async_read(*socket, asio::buffer(*sizeBuf),
						[socket, sizeBuf, onDone](std::error_code ec, std::size_t) {
							if (ec) {
								onDone(ec, {});
								return;
							}
							// Step 4: read block header data
							uint64_t headerSize;
							takeBytesInto(headerSize, *sizeBuf);
							auto headerBytes = std::make_shared<std::vector<uint8_t>>(headerSize);
							asio::async_read(*socket, asio::buffer(*headerBytes),
								[headerBytes, onDone](std::error_code ec, std::size_t) {
									if (ec) {
										onDone(ec, {});
										return;
									}
									onDone({}, { formatBlockHeader(*headerBytes) });
								});
						});
				});
		});
}

void requestBlock(std::shared_ptr<asio::ip::tcp::socket> socket, Array256_t blockHash, std::function<void(std::error_code, Block)> onDone)
{
	// Step 1: send request message type
	auto requestMsgType = std::make_shared<uint8_t>(ProtocolMessage::GetBlock);
	asio::async_write(*socket, asio::buffer(requestMsgType.get(), 1),
		[socket, blockHash, onDone](std::error_code ec, std::size_t) {
			if (ec) {
				onDone(ec, {});
				return;
			}

			// Step 2: send block hash
			auto hashBuf = std::make_shared<Array256_t>();
			asio::async_write(*socket, asio::buffer(blockHash),
				[socket, hashBuf, onDone](std::error_code ec, std::size_t) {
					if (ec) {
						onDone(ec, {});
						return;
					}

					// Step 3: read block size
					auto sizeBuf = std::make_shared<std::array<uint8_t, 8>>();
					asio::async_read(*socket, asio::buffer(*sizeBuf),
						[socket, sizeBuf, onDone](std::error_code ec, std::size_t) {
							if (ec) {
								onDone(ec, {});
								return;
							}

							// Step 4: read block data
							uint64_t blockSize;
							takeBytesInto(blockSize, *sizeBuf);
							auto blockBytes = std::make_shared<std::vector<uint8_t>>(blockSize);
							asio::async_read(*socket, asio::buffer(*blockBytes),
								[blockBytes, onDone](std::error_code ec, std::size_t) {
									if (ec) {
										onDone(ec, {});
										return;
									}

									onDone({}, { formatBlock(*blockBytes) });
								});
						});
				});
		});
}

void requestMempool(std::shared_ptr<asio::ip::tcp::socket> socket)
{
	// Implementation omitted for brevity
}

void broadcastBlockToPeers(Block block, std::shared_ptr<asio::ip::tcp::socket> socket)
{
	// Implementation omitted for brevity
}

void broadcastTransaction(std::shared_ptr<asio::ip::tcp::socket> socket, Tx transaction)
{
	// Implementation omitted for brevity
}

// ===========================================================
// Handling incoming connections
// ===========================================================

void respondToPing(std::shared_ptr<asio::ip::tcp::socket> socket) {

	// Step 1: send ping reply
	auto pingReply = std::make_shared<uint8_t>(1);
	asio::async_write(*socket, asio::buffer(pingReply.get(), 1),
		[socket, pingReply](std::error_code ec, std::size_t) {
			if (ec) {
				socket->close();
				return;
			}
		});
}

void respondToGetHeader(std::shared_ptr<asio::ip::tcp::socket> socket) {

	auto blockHash = std::make_shared<Array256_t>();
	asio::async_read(*socket, asio::buffer(*blockHash),
		[socket, blockHash](std::error_code ec, std::size_t) {
			if (ec) {
				socket->close();
				return;
			}
			// Step 2: send block header size
			auto blockHeaderBytes = std::make_shared<std::vector<uint8_t>>(serialiseBlockHeader(formatBlockHeader(readBlockFileHeader(*blockHash))));
			auto blockHeaderSize = blockHeaderBytes->size();
			auto blockHeaderSizeBytes = std::make_shared<uint64_t>();
			appendBytes(*blockHeaderSizeBytes, blockHeaderSize);
			asio::async_write(*socket, asio::buffer(blockHeaderSizeBytes.get(), 8),
				[socket, blockHeaderBytes](std::error_code ec, std::size_t) {
					if (ec) {
						socket->close();
						return;
					}
					// Step 3: send block header data
					asio::async_write(*socket, asio::buffer(*blockHeaderBytes),
						[socket](std::error_code ec, std::size_t) {
							if (ec) {
								socket->close();
								return;
							}
						});
				});
		});
}

void respondToGetBlock(std::shared_ptr<asio::ip::tcp::socket> socket) {

	// Step 1: read block hash
	auto blockHash = std::make_shared<Array256_t>();
	asio::async_read(*socket, asio::buffer(*blockHash),
		[socket, blockHash](std::error_code ec, std::size_t) {
			if (ec) {
				socket->close();
				return;
			}
			// Step 2: send block size
			auto blockData = std::make_shared<std::vector<uint8_t>>(readBlockFile(*blockHash));
			auto blockSize = blockData->size();
			auto blockSizeBuf = std::make_shared<uint64_t>(blockSize);
			appendBytes(*blockSizeBuf, blockSize);
			asio::async_write(*socket, asio::buffer(blockSizeBuf.get(), sizeof(*blockSizeBuf)),
				[socket, blockData](std::error_code ec, std::size_t) {
					if (ec) {
						socket->close();
						return;
					}

					// Step 3: send block data
					asio::async_write(*socket, asio::buffer(*blockData),
						[socket](std::error_code ec, std::size_t) {
							if (ec) {
								socket->close();
								return;
							}
						});
				});
		});
}

void respondToGetMempool(std::shared_ptr<asio::ip::tcp::socket> socket) {
	// Implementation omitted for brevity
}

void respondToBroadcastBlock(std::shared_ptr<asio::ip::tcp::socket> socket) {

	// Step 1: read block size
	auto blockSizeBuf = std::make_shared<std::array<uint8_t, 8>>();
	asio::async_read(*socket, asio::buffer(*blockSizeBuf),
		[socket, blockSizeBuf](std::error_code ec, std::size_t) {
			if (ec) {
				socket->close();
				return;
			}

			uint64_t blockSize;
			takeBytesInto(blockSize, *blockSizeBuf);

			// Step 2: read block data
			auto blockData = std::make_shared<std::vector<uint8_t>>(blockSize);
			asio::async_read(*socket, asio::buffer(*blockData),
				[socket, blockData](std::error_code ec, std::size_t) {
					if (ec) {
						socket->close();
						return;
					}
					Block newBlock = formatBlock(*blockData);
					if (validateBlock(newBlock)) {
						addBlock(newBlock);
						broadcastBlockToPeers(newBlock, socket);
					}
					else if (blockExists(getBlockHash(newBlock))) {
						// Block already exists, do nothing
					}
					else {
						socket->close();
					}
				});
		}
}

void respondToBroadcastTransaction(std::shared_ptr<asio::ip::tcp::socket> socket) {

	// Step 1: read transaction size
	auto txSizeBuf = std::make_shared<std::array<uint8_t, 8>>();
	asio::async_read(*socket, asio::buffer(*txSizeBuf),
		[socket, txSizeBuf](std::error_code ec, std::size_t) {
			if (ec) {
				socket->close();
				return;
			}
			uint64_t txSize;
			takeBytesInto(txSize, *txSizeBuf);

			// Step 2: read transaction data
			auto txData = std::make_shared<std::vector<uint8_t>>(txSize);
			asio::async_read(*socket, asio::buffer(*txData),
				[socket, txData](std::error_code ec, std::size_t) {
					if (ec) {
						socket->close();
						return;
					}

					Tx newTx = formatTx(*txData);
					if (validateTx(newTx)) {
						mempool.push_back(newTx);
						broadcastTransaction(socket, newTx);
					}
					else {
						socket->close();
					}
				});
		});
}

// Start accepting incoming connections
void startAccepting(asio::ip::tcp::acceptor& acceptor) {
	acceptor.async_accept(
		[&acceptor](std::error_code ec, asio::ip::tcp::socket socket) {
			if (!ec) {
				auto sock = std::make_shared<asio::ip::tcp::socket>(std::move(socket));

				PeerAddress peerAddr;
				peerAddr.address = sock->remote_endpoint().address().to_string();
				peerAddr.port = sock->remote_endpoint().port();

				// Read first byte to determine message type or handshake
				auto msgType = std::make_shared<uint8_t>();
				asio::async_read(*sock, asio::buffer(msgType.get(), 1),
					[sock, msgType, peerAddr](std::error_code ec, std::size_t) {
						if (ec) {
							sock->close();
							return;
						}

						// If peer sends 1 → perform handshake
						if (*msgType == 1) {
							respondHandshake(sock);
							return;
						}

						// Otherwise, check if peer already exists
						if (peers.find(peerAddr) != peers.end()) {
							// Update last seen
							peers[peerAddr].lastSeen = getCurrentTimestamp();

							// Handle protocol messages
							switch (static_cast<ProtocolMessage>(*msgType)) {
							case ProtocolMessage::Ping: respondToPing(sock); break;
							case ProtocolMessage::GetBlock: respondToGetBlock(sock); break;
							default: sock->close(); break;
							}
						}
						else {
							// New peer sending something other than handshake → close
							sock->close();
						}
					}
				);
			}
			// Continue accepting next connections
			startAccepting(acceptor);
		}
	);
}


int main() {
	asio::io_context ioContext;
	asio::ip::tcp::acceptor acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

	// Start accepting connections
	startAccepting(acceptor);

	// Run the event loop
	ioContext.run();
}
