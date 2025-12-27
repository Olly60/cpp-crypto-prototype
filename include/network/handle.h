#pragma once
#include <asio.hpp>

asio::awaitable<void> handleGetHeader(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleGetBlock(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleHandshake(asio::ip::tcp::socket& socket);

asio::awaitable<void> handlePing(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleGetHeaders(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleGetMempool(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleGetPeers(asio::ip::tcp::socket& socket)

// ============================================
// Handle new data
// ============================================

asio::awaitable<void> handleNewBlock(asio::ip::tcp::socket& socket);

asio::awaitable<void> handleNewTx(asio::ip::tcp::socket& socket);

