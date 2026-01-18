#include <vector>
#include "network/request.h"
#include "user_commands.h"
#include <iostream>
#include <ranges>
#include <thread>
#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/ip/tcp.hpp>
#include <boost/multiprecision/number.hpp>
#include <sodium/crypto_sign.h>
#include "block_work.h"
#include "node.h"
#include "tip.h"
#include "verify.h"
#include "wallet.h"
#include "network/network_main.h"
#include "storage/utxo_storage.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"

asio::awaitable<void> handleUserNetworkCommand(const std::vector<std::string>& parts)
{
    try
    {
        if (parts.size() < 4)
            co_return;

        asio::ip::tcp::socket socket(ioCtx);

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
            co_return;
        }

        if (parts[1] == "get_mempool")
        {
            if (co_await requestMempool(socket))
            {
                std::cout << "Got mempool from: " << remote << "\n";
            }
            else { std::cout << "Failed to get whole mempool from: " << remote << "\n"; }
            co_return;
        }

        if (parts[1] == "get_peers")
        {
            if (co_await requestPeers(socket))
            {
                std::cout << "Got peers from: " << remote << "\n";
            }
            else { std::cout << "Couldn't get peers from: " << remote << "\n"; }
            co_return;
        }

        if (parts[1] == "handshake")
        {
            if (co_await requestHandshake(socket))
            {
                std::cout << "Successful handshake with: " << remote << "\n";
            }
            else { std::cout << "Failed to handshake with: " << remote << "\n"; }
            co_return;
        }

        if (parts[1] == "sync")
        {
            if (co_await syncIfBetter(socket))
            {
                std::cout << "Synced with: " << remote << "\n";
            }
            else { std::cout << "Didn't sync with: " << remote << "\n"; }
            co_return;
        }

        if (parts[1] == "headers")
        {
            bool r = (co_await requestHeaders(socket)).empty();
            if (r)
            {
                std::cout << "headers : " << remote << "\n";
            }
            else { std::cout << "Didn't headers: " << remote << "\n"; }
            co_return;
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

std::optional<std::jthread> miningThread;

void handleUserCommand(const std::string& input)
{
    std::vector<std::string> parts;
    std::istringstream stream(input);
    std::string part;
    while (stream >> part)
    {
        parts.push_back(part);
    }

    part.resize(4); // Resize to the longest command to prevent accessing out-of-bounds memory

    if (parts[0] == "help")
    {
        std::cout << "Commands:\n";
        std::cout << "peers ping <ip> <port>\n";
        std::cout << "peers handshake <ip> <port>\n";
        std::cout << "peers sync <ip> <port>\n";
        std::cout << "peers get_mempool\n";
        std::cout << "peers get_peers\n";
        std::cout << "chain_info\n";
        std::cout << "mine start <reward_address>\n";
        std::cout << "mine stop\n";
        std::cout << "tx <sender_secret_key> <recipient_address> <amount>\n";
        std::cout << "known_peers\n";
        std::cout << "unknown_peers\n";
        std::cout << "block_info <block_hash>\n";
        std::cout << "wallet add <recipient_address>\n";
        std::cout << "wallet remove <recipient_address>\n";
        std::cout << "wallet amount <recipient_address>\n";
        std::cout << "wallet list\n";
        std::cout << "keyset\n";
    }

    if (parts[0] == "peers")
    {
        asio::co_spawn(ioCtx, handleUserNetworkCommand(parts), asio::detached);
    }

    if (parts[0] == "chain_info")
    {
        BytesBuffer tipBuf;
        tipBuf.writeArray256(getTipHash());
        std::cout << "Current tip hash: " << bytesToHex(tipBuf) << "\n";
        BytesBuffer genBuf;
        genBuf.writeArray256(getGenesisBlockHash());
        std::cout << "Genesis hash: " << bytesToHex(genBuf) << "\n";
        std::cout << "Length: " << std::to_string(tryGetBlockIndex(getTipHash())->height + 1) << "\n";
        BytesBuffer chainWorkBuf;
        chainWorkBuf.writeArray256(tryGetBlockIndex(getTipHash())->chainWork);
        std::cout << "Chain work: " << bytesToHex(chainWorkBuf) << "\n";
    }


    if (parts[0] == "mine")
    {
        if (parts[1] == "start")
        {
            std::cout << "Started mining blocks\n";
            if (miningThread)
            {
                std::cout << "Already mining\n";
                return;
            };
            Array256_t rewardAddr = hexToBytes(parts[2]).readArray256();
            miningThread = std::jthread(mineBlocks, rewardAddr);
            return;
        }

        if (parts[1] == "stop")
        {
            miningThread.reset();
            std::cout << "Stopped mining blocks\n";
        }
    }

    if (parts[0] == "tx")
    {
        auto sk = hexToBytes(parts[1]).readArray512();
        Array256_t pk;
        crypto_sign_ed25519_sk_to_pk(pk.data(), sk.data());
        auto newTx = makeTx(wallets[pk], sk,
                            hexToBytes(parts[2]).readArray256(), std::stoi(parts[3]));

        if (!verifyTx(newTx))
        {
            std::cerr << "Invalid transaction\n";
            return;
        }
        mempool.insert({getTxHash(newTx), newTx});
        asio::co_spawn(ioCtx, broadcastNewTx(ioCtx, newTx), asio::detached);
        std::cout << "Transaction sent to mempool\n";
    }

    if (parts[0] == "known_peers")
    {
        for (const auto& peer : knownPeers)
        {
            std::cout << peer.first.to_string() << " " << peer.second.port << "\n";
        }
    }

    if (parts[0] == "unknown_peers")
    {
        for (auto& peer : unknownPeers)
        {
            std::cout << peer.address().to_string() << " " << peer.port() << "\n";
        }
    }

    if (parts[0] == "block_info")
    {
        Array256_t hash = hexToBytes(parts[1]).readArray256();
        std::cout << "Block height: " << (tryGetBlockIndex(hash)->height + 1) << "\n";
        std::cout << "Block work: " << bytesToHex(tryGetBlockIndex(hash)->chainWork) << "\n";
    }

    if (parts[0] == "wallet")
    {
        if (parts[1] == "add")
        {
            auto pubKey = hexToBytes(parts[2]).readArray256();
            wallets[pubKey] = getUtxosForRecipient(pubKey);
            std::cout << "Added wallet" << "\n";
        }

        if (parts[1] == "remove")
        {
            auto pubKey = hexToBytes(parts[2]).readArray256();
            wallets.erase(pubKey);
            std::cout << "Removed wallet" << "\n";
        }

        if (parts[1] == "amount")
        {
            auto pubKey = hexToBytes(parts[2]).readArray256();
            uint64_t amount = 0;
            for (auto& value : wallets[pubKey])
            {
                amount += tryGetUtxo(value)->amount;
            }
            std::cout << amount << "\n";
        }

        if (parts[1] == "list")
        {
            for (auto& wallet : wallets)
            {
                uint64_t amount = 0;
                for (auto& value : wallets[wallet.first])
                {
                    amount += tryGetUtxo(value)->amount;
                }
                std::cout << bytesToHex(wallet.first) << " ";
                std::cout << amount << "\n";
            }
        }
    }
    if (parts[0] == "keyset")
    {
        Array256_t pubKey;
        Array512_t secKey;
        crypto_sign_keypair(pubKey.data(), secKey.data());

        BytesBuffer keysBuf;
        keysBuf.writeArray256(pubKey);
        std::cout << "Public key is: " << bytesToHex(keysBuf) << "\n";

        keysBuf.clear();
        keysBuf.writeArray512(secKey);
        std::cout << "Secret key is: " << bytesToHex(keysBuf) << "\n";
    }

    if (parts[0] == "mempool")
    {
        for (const auto& hash : mempool | std::views::keys)
        {
            std::cout << bytesToHex(hash) << "\n";
        }
    }
}
