#include <vector>
#include "network/request.h"
#include "user_commands.h"
#include <iostream>
#include <ranges>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <asio/co_spawn.hpp>
#include <asio/ip/tcp.hpp>

#include "block_work.h"
#include "node.h"
#include "tip.h"
#include "network/network_main.h"
#include "storage/block/block_indexes.h"
#include "block.h"

asio::io_context userIo;

asio::awaitable<void> handleUserNetworkCommand(const std::vector<std::string>& parts)
{
    try
    {
        if (parts.size() < 4)
            co_return;

        asio::ip::tcp::socket socket(userIo);

        co_await socket.async_connect(
            asio::ip::tcp::endpoint(
                asio::ip::make_address(parts[2]),
                static_cast<uint16_t>(std::stoi(parts[3]))
            ),
            asio::use_awaitable
        );

        auto remote = socket.remote_endpoint().address().to_string();

        if (parts[1] == "ping")
        {
            if (co_await requestPing(socket))
            {
                std::cout << "Pong from: " << remote << "\n";
            }
            else { std::cout << "No pong from: " << remote << "\n"; }
        }
        else if (parts[1] == "get_mempool")
        {
            if (co_await requestMempool(socket))
            {
                std::cout << "Got mempool from: " << remote << "\n";
            }
            else { std::cout << "Failed to get whole mempool from: " << remote << "\n"; }
        }
        else if (parts[1] == "get_peers")
        {
            if (co_await requestPeers(socket))
            {
                std::cout << "Got peers from: " << remote << "\n";
            }
            else { std::cout << "Couldn't get peers from: " << remote << "\n"; }
        }
        else if (parts[1] == "handshake")
        {
            if (co_await requestHandshake(socket))
            {
                std::cout << "Successful handshake with: " << remote << "\n";
            }
            else { std::cout << "Failed to handshake with: " << remote << "\n"; }
        }
        else if (parts[1] == "sync")
        {
            if (co_await syncIfBetter(socket))
            {
                std::cout << "Synced with: " << remote << "\n";
            }
            else { std::cout << "Didn't sync with: " << remote << "\n"; }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "network command error: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "unknown network command error\n";
    }
}

void handleUserCommand(const std::string& input)
{
    std::vector<std::string> parts;
    std::istringstream stream(input);
    std::string part;
    while (stream >> part)
    {
        parts.push_back(part);
    }

    if (parts[0] == "peers")
    {
        asio::co_spawn(userIo, handleUserNetworkCommand(parts), asio::detached);
        userIo.run();
        userIo.restart();
        return;
    }

    if (parts[0] == "chain_info")
    {
        std::cout << "Length: " << std::to_string(tryGetBlockIndex(getTipHash())->height + 1) << "\n";
        BytesBuffer chainWorkBuf;
        chainWorkBuf.writeArray256(tryGetBlockIndex(getTipHash())->chainWork);
        std::cout << "Chain work: " << bytesToHex(chainWorkBuf) << "\n";
    }


    if (parts[0] == "mine")
    {
        if (parts[1] == "start" && isMining == false)
        {
            std::cout << "Started mining blocks\n";
            if (isMining == true)
            {
                std::cout << "Already mining\n";
                return;
            };
            isMining = true;
            std::thread miner(mineBlocks, hexToBytes(parts[2]).readArray256());
            miner.detach();
        }
        else if (parts[1] == "stop")
        {
            isMining = false;
            std::cout << "Stopped mining blocks\n";
        }
    }

    if (parts[0] == "new_tx")
    {
        BytesBuffer buf;
        buf.writeArray256(getBlock(getTipHash())->header.difficulty);
        std::cout << bytesToHex(buf) << "\n";
    }

    if (parts[0] == "known_peers")
    {
        for (const auto& key : knownPeers | std::views::keys)
        {
            std::cout << key.address().to_string() << " " << key.port() << "\n";
        }
    }

    if (parts[0] == "unknown_peers")
    {
        for (auto& peer : unknownPeers)
        {
            std::cout << peer.address().to_string() << " " << peer.port() << "\n";
        }
    }
}
