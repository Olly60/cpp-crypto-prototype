#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "tip.h"
#include "network/request.h"

#include <iostream>

#include "verify.h"
#include "network/network_utils.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

asio::awaitable<bool> requestPeers(asio::ip::tcp::socket& socket)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::GetPeers) != 0) co_return false;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::GetPeers), asio::use_awaitable);

        // Read peers amount
        auto peersAmount = co_await readU64Tcp(socket);

        unknownPeers.reserve(peersAmount + knownPeers.size());

        for (uint64_t i = 0; i < peersAmount; ++i)
        {
            asio::ip::tcp::endpoint peerAddr;
            // Read type
            uint8_t type;
            co_await asio::async_read(socket, asio::buffer(&type, 1), asio::use_awaitable);
            if (type == 4)
            {
                std::array<uint8_t, 4> addr{};
                co_await asio::async_read(socket, asio::buffer(addr.data(), addr.size()), asio::use_awaitable);
                peerAddr.address(asio::ip::address_v4(addr));
            }
            else if (type == 6)
            {
                std::array<uint8_t, 16> addr{};
                co_await asio::async_read(socket, asio::buffer(addr.data(), addr.size()), asio::use_awaitable);
                peerAddr.address(asio::ip::address_v6(addr));
            }
            else continue;

            BytesBuffer port(2);
            co_await asio::async_read(socket, asio::buffer(port.data(), port.size()), asio::use_awaitable);
            peerAddr.port(port.readU16());

            unknownPeers.insert(peerAddr);
        }
        co_return true;
    }
    catch (...)
    {
        co_return false;
    }
}

asio::awaitable<bool> requestHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::Handshake) != 0) co_return false;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::Handshake), asio::use_awaitable);

        // Write our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake.data(), myHandshake.size()), asio::use_awaitable);

        // Read peer handshake
        BytesBuffer buffer(calculateHandshakeSize());
        co_await asio::async_read(socket, asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);

        const Handshake theirHandshake = parseHandshake(buffer);
        if (!isValidHandshake(theirHandshake))
        {
            co_return false;
        }

        // Write verack
        uint8_t myVerack = 0x01;
        co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

        // Read verack
        uint8_t theirVerack;
        co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
        if (theirVerack != 0x01)
        {
            co_return false;
        }

        knownPeers.insert({
            socket.remote_endpoint(), {theirHandshake.services, getCurrentTimestamp(), theirHandshake.relay}
        });
        co_return true;
    }
    catch (...)
    {
        co_return false;
    }
}

asio::awaitable<bool> requestPing(asio::ip::tcp::socket& socket)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::Ping) != 0) co_return false;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::Ping), asio::use_awaitable);

        // Read pong
        uint8_t pong;
        co_await asio::async_read(socket, asio::buffer(&pong, 1), asio::use_awaitable);

        // Update last seen if valid
        if (pong == 0x01)
        {
            knownPeers[socket.remote_endpoint()].lastSeen = getCurrentTimestamp();
            co_return true;
        }

        if (pong == 0x00) // Else remove from peers
        {
            knownPeers.erase(socket.remote_endpoint());
        }
        co_return false;
    }
    catch (...)
    {
        co_return false;
    }
}

asio::awaitable<std::optional<BlockHeader>> requestBlockHeader(
    asio::ip::tcp::socket& socket,
    const Array256_t& blockHash
)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::GetHeader) != 0) co_return std::nullopt;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::GetHeader), asio::use_awaitable);

        // Write header hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have header
        uint8_t hasHeader;
        co_await asio::async_read(socket, asio::buffer(&hasHeader, 1), asio::use_awaitable);
        if (hasHeader == 0) { co_return std::nullopt; }

        // Read header
        BytesBuffer headerBytes(calculateBlockHeaderSize());
        co_await asio::async_read(socket, asio::buffer(headerBytes.data(), headerBytes.size()), asio::use_awaitable);

        co_return parseBlockHeader(headerBytes);
    }
    catch (...)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}


asio::awaitable<std::optional<ChainBlock>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::GetBlock) != 0) co_return std::nullopt;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::GetBlock), asio::use_awaitable);

        // Block hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have block
        uint8_t hasBlock;
        co_await asio::async_read(socket, asio::buffer(&hasBlock, 1), asio::use_awaitable);
        if (hasBlock == 0) { co_return std::nullopt; }

        // Read size
        const uint64_t blockSize = co_await readU64Tcp(socket);

        // Read block
        BytesBuffer blockBytes(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);

        co_return parseBlock(blockBytes);
    }
    catch (const std::exception&)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}

asio::awaitable<std::vector<BlockHeader>> requestHeaders(asio::ip::tcp::socket& socket)
{
    try
    {
        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::GetHeaders) != 0) co_return std::vector<BlockHeader>{};

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::GetHeaders), asio::use_awaitable);

        // Get block tip height
        auto tipHeight = getTipHeight();

        // Make list of block hashes with (Ancestor -> Tip)
        std::vector<Array256_t> blockHashes;

        // Add genesis block hash at the start
        blockHashes.push_back(getGenesisBlockHash());

        // Exponential hash collection (Ancestor -> Tip)
        for (uint64_t i = 1; i < tipHeight; i++)
        {
            blockHashes.push_back(*tryGetHeightHash(i));
        }

        // Add tip block hash at the end
        blockHashes.push_back(getTipHash());

        // Write amount of hashes
        co_await writeU64Tcp(socket, blockHashes.size());

        // Write hashes
        for (const auto& hash : blockHashes)
        {
            co_await asio::async_write(socket, asio::buffer(hash), asio::use_awaitable);
        }

        // Read header amount
        uint64_t headerAmount = co_await readU64Tcp(socket);

        // Read headers
        std::vector<BlockHeader> headers;
        for (uint64_t i = 0; i < headerAmount; i++)
        {
            BytesBuffer headerBytes(calculateBlockHeaderSize());
            co_await asio::async_read(socket, asio::buffer(headerBytes.data(), headerBytes.size()),
                                      asio::use_awaitable);
            headers.push_back(parseBlockHeader(headerBytes));
        }

        // Drop leading headers we already have (chronological order: oldest -> newest)
        std::vector<BlockHeader>::difference_type drop = 0;

        while (drop < static_cast<std::vector<BlockHeader>::difference_type>(headers.size()) &&
            std::filesystem::exists(getBlockFilePath(getBlockHeaderHash(headers[drop]))))
        {
            ++drop;
        }

        if (drop > 0)
        {
            headers.erase(headers.begin(), headers.begin() + drop);
        }


        co_return headers;
    }
    catch (...)
    {
        co_return std::vector<BlockHeader>{};
    }
}

asio::awaitable<bool> requestMempool(asio::ip::tcp::socket& socket)
{
    try
    {

        // Check node offers service
        auto peerServices = knownPeers[socket.remote_endpoint()].services;
        if ((peerServices & Services::FullNode) != 0 || (peerServices & Services::GetMempool) != 0) co_return false;

        // Write message type
        co_await asio::async_write(socket, asio::buffer(ProtocolMessage::GetMempool), asio::use_awaitable);

        // Read inv size
        const uint64_t invSize = co_await readU64Tcp(socket);

        // Read inv
        std::vector<Array256_t> theirInv(invSize);

        for (uint64_t i = 0; i < invSize; i++)
        {
            Array256_t hash;
            co_await asio::async_read(socket, asio::buffer(hash), asio::use_awaitable);
            theirInv.push_back(hash);
        }

        // Find missing
        std::vector<Array256_t> missingInv;
        missingInv.reserve(invSize);
        for (auto& hash : theirInv)
        {
            if (!mempool.contains(hash))
            {
                missingInv.push_back(hash);
            }
        }

        // Write missing size
        co_await writeU64Tcp(socket, missingInv.size());

        // Ask for missing transactions
        for (const auto& hash : missingInv)
        {
            co_await asio::async_write(socket, asio::buffer(hash), asio::use_awaitable);
        }

        // Get each transaction
        std::vector<Tx> txs;
        txs.reserve(invSize);
        for (uint64_t i = 0; i != missingInv.size(); i++)
        {
            // Read size
            const uint64_t txSize = co_await readU64Tcp(socket);

            // Read transaction
            BytesBuffer txBytes(txSize);
            co_await asio::async_read(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);

            txs.push_back(parseTx(txBytes));
        }

        // Add their mempool to local mempool
        MempoolMap theirMempool;
        theirMempool.reserve(txs.size());
        for (auto& tx : txs)
        {
            if (!verifyTx(tx)) co_return false;
            auto hashTx = getTxHash(tx);
            mempool.insert({hashTx, tx});
        }
        co_return true;
    }
    catch (...)
    {
        co_return false;
    }
}
