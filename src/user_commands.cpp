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

        auto addr = socket.remote_endpoint().address().to_string();

        if (parts[1] == "ping")
        {
            if (co_await requestPing(socket))
            {
                std::cout << "Pong from: " << addr << "\n";
            }
            else { std::cout << "No pong from: " << addr << "\n"; }
            co_return;
        }

        if (parts[1] == "get_mempool")
        {
            if (co_await requestMempool(socket))
            {
                std::cout << "Got mempool from: " << addr << "\n";
            }
            else { std::cout << "Failed to get whole mempool from: " << addr << "\n"; }
            co_return;
        }

        if (parts[1] == "get_peers")
        {
            if (co_await requestPeers(socket))
            {
                std::cout << "Got peers from: " << addr << "\n";
            }
            else { std::cout << "Couldn't get peers from: " << addr << "\n"; }
            co_return;
        }

        if (parts[1] == "handshake")
        {
            if (co_await requestHandshake(socket))
            {
                std::cout << "Successful handshake with: " << addr << "\n";
            }
            else { std::cout << "Failed to handshake with: " << addr << "\n"; }
            co_return;
        }

        if (parts[1] == "sync")
        {
            std::cout << "Attempting sync with: " << addr << "\n";
            if (co_await syncIfBetter(socket))
            {
                std::cout << "Synced with: " << addr << "\n";
            }
            else { std::cout << "Didn't sync with: " << addr << "\n"; }
            co_return;
        }

        if (parts[1] == "headers")
        {
            bool r = (co_await requestHeaders(socket)).empty();
            if (r)
            {
                std::cout << "headers : " << addr << "\n";
            }
            else { std::cout << "Didn't headers: " << addr << "\n"; }
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
        std::cout <<
                "======================================================================\n"
                " NETWORK\n"
                "  peers handshake <ip> <port> Connect and add to known peers\n"
                "  peers ping <ip> <port>      Check if a peer is active\n"
                "  peers sync <ip> <port>      Sync chain/mempool if peer is ahead\n"
                "  peers get_mempool           Request mempool from network\n"
                "  peers get_peers             Discover new peers\n"
                "  known_peers                 List verified peers\n"
                "  unknown_peers               List unverified peers\n"
                "\n"
                " MINING & CHAIN\n"
                "  chain_info                  Display chain height and work\n"
                "  mine start <addr>           Start mining to <addr>\n"
                "  mine stop                   Halt mining operations\n"
                "  block_info <hash>           Show details for a specific block\n"
                "\n"
                " WALLET & TRANSACTIONS\n"
                "  tx <sk> <to> <amt>          Send transaction (SecretKey, To, Amount)\n"
                "  wallet add <addr>           Track a wallet's balance\n"
                "  wallet remove <addr>        Stop tracking a wallet\n"
                "  wallet amount <addr>        Show balance of one wallet\n"
                "  wallet list                 Show all tracked balances\n"
                "  keyset                      Generate a new keypair\n"
                "\n"
                " SYSTEM\n"
                "  exit                        Shutdown and save state\n"
                "======================================================================\n";
    }

    if (parts[0] == "peers")
    {
        asio::co_spawn(ioCtx, handleUserNetworkCommand(parts), asio::detached);
    }

    if (parts[0] == "chain_info")
    {
        auto tipHash = getTipHash();
        std::cout << "Current tip hash: " << bytesToHex(tipHash.data(), tipHash.size()) << "\n";
        auto genesisHash = getGenesisHash();
        std::cout << "Genesis hash: " << bytesToHex(genesisHash.data(), genesisHash.size()) << "\n";
        std::cout << "Length: " << (tryGetBlockIndex(getTipHash())->height + 1) << "\n";
        auto chainWork = tryGetBlockIndex(getTipHash())->chainWork;
        std::cout << "Chain work: " << bytesToHex(chainWork.data(), chainWork.size()) << "\n";
        auto difficulty = getBlockHeader(tipHash)->difficulty;
        std::cout << "Current difficulty target: " << bytesToHex(difficulty.data(), difficulty.size()) << "\n";
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

        if (!wallets.contains(pk))
        {
            std::cerr << "Wallet not found\n";
            return;
        }

        std::unordered_set<UTXOId, UTXOIdHash> validUtxos = wallets[pk];

        for (auto& val : mempool | std::views::values)
        {
            for (const auto& txInput : val.txInputs)
            {
                validUtxos.erase(txInput.utxoId);
            }
        }

        auto newTx = makeTx(wallets[pk], sk, hexToBytes(parts[2]).readArray256(), std::stoi(parts[3]));

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
        auto blockIndex = tryGetBlockIndex(hash);
        if (!blockIndex)
        {
            std::cout << "Block not in chain\n";
            return;
        }
        std::cout << "Block height: " << blockIndex->height << "\n";
        auto chainWork = blockIndex->chainWork;
        std::cout << "Block chain work: " << bytesToHex(chainWork.data(), chainWork.size()) << "\n";
        auto block = getBlock(hash);
        std::cout << "Previous block hash: " << bytesToHex(block->header.prevBlockHash.data(),
                                                           block->header.prevBlockHash.size()) << "\n";
    }

    if (parts[0] == "wallets")
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
            std::cout << "Balance" << amount << "\n";
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
                std::cout << bytesToHex(wallet.first.data(), wallet.first.size()) << " ";
                std::cout << amount << "\n";
            }
        }
    }
    if (parts[0] == "keyset")
    {
        Array256_t pk;
        Array512_t sk;
        crypto_sign_keypair(pk.data(), sk.data());
        std::cout << "Public key is: " << bytesToHex(pk.data(), pk.size()) << "\n";
        std::cout << "Secret key is: " << bytesToHex(sk.data(), sk.size()) << "\n";
    }

    if (parts[0] == "mempool")
    {
        for (const auto& tx : mempool)
        {
            std::cout << bytesToHex(tx.first.data(), tx.first.size()) << "\n";
        }
    }
}
