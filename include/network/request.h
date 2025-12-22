#pragma once

asio::awaitable<void> requestHandshake(asio::ip::tcp::socket& socket);

asio::awaitable<void> requestPing(asio::ip::tcp::socket& socket);

asio::awaitable<std::optional<BlockHeader>> requestBlockHeader(
    asio::ip::tcp::socket& socket,
    const Array256_t& blockHash
);

asio::awaitable<std::optional<Block>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash);

asio::awaitable<std::optional<std::vector<BlockHeader>>> requestHeaders(asio::ip::tcp::socket& socket);

asio::awaitable<std::optional<std::vector<Block>>> requestBlocks(asio::ip::tcp::socket& socket, const std::vector<const Array256_t>& blockHashes);

asio::awaitable<std::optional<std::vector<Tx>>> requestMempool(asio::ip::tcp::socket& socket);