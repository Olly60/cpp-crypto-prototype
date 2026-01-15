#include "block_work.h"
#include <iostream>
#include <ranges>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include "node.h"
#include "tip.h"
#include "transaction.h"
#include "network/network_main.h"
#include "storage/utxo_storage.h"
#include "block.h"
// Get Block work
Array256_t getBlockWork(const Array256_t& difficulty)
{
    Array256_t work;
    work.fill(0xff);
    for (size_t i = 0; i < difficulty.size(); ++i)
    {
        for (size_t j = 0; j < 8; ++j)
        {
            if (i >> j != 0) work = shiftRightBE(work);
        }
    }

    return work;
}

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b)
{
    Array256_t totalWork;
    uint8_t carry = 0;
    for (size_t i = totalWork.size(); i > 0; --i)
    {
        totalWork[i - 1] = a[i - 1] + b[i - 1];
        totalWork[i - 1] += carry;
        carry = a[i - 1] + b[i - 1] < a[i - 1] || a[i - 1] + b[i - 1] < b[i - 1] ? 1 : 0;
    }
    return totalWork;
}

// Shift right (harder)
Array256_t shiftRightBE(const Array256_t& arr)
{
    auto newArr = arr;
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (newArr[i] != 0)
        {
            newArr[i] >>= 1;
            break;
        }
    }
    return newArr;
}

// Shift left (easier)
Array256_t shiftLeftBE(const Array256_t& arr)
{
    auto newArr = arr;

    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (newArr[i] != 0 || newArr[i + 1] == 0xff)
        {
            newArr[i] <<= 1;
            newArr[i] |= 1;
            break;
        }
    }
    return newArr;
}


asio::io_context miningIo;

ChainBlock newBlock(Array256_t pubKey)
{
    auto currentTip = getTipHash();
    ChainBlock block;
    block.header.prevBlockHash = getTipHash();
    block.header.timestamp = getCurrentTimestamp();

    // Difficulty adjustment (target-based)
    auto currentTipHeader = getBlockHeader(currentTip);

    uint64_t timeDelta = currentTipHeader->timestamp - getBlockHeader(currentTipHeader->prevBlockHash)->timestamp;

    block.header.difficulty = (timeDelta < 600)
                                  ? shiftLeftBE(currentTipHeader->difficulty)
                                  : shiftRightBE(currentTipHeader->difficulty);

    uint64_t size = 0;
    for (auto& val : mempool | std::views::values)
    {
        size += serialiseTx(val).size();

        if (size > MAX_TX_SIZE - calculateBlockHeaderSize()) break;
        block.txs.push_back(val);
    }

    uint64_t totalFees = 0;
    for (auto& tx : block.txs)
    {
        uint64_t totalInputAmount = 0;
        for (const auto& input : tx.txInputs)
        {
            auto utxoInDb = tryGetUtxo(input);
            totalInputAmount += utxoInDb->amount;
        }

        uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1));
        totalFees += txFee;
    }

    Tx coinbaseTx;
    coinbaseTx.txOutputs.push_back({totalFees + 5000000000, pubKey});

    block.txs.insert(block.txs.begin(), coinbaseTx);

    block.header.merkleRoot = getMerkleRoot(block.txs);
    return block;
}

void mineBlocks(Array256_t pubKey)
{
    if (isMining == true) return;
    auto generateNonce = [
            engine = std::mt19937{std::random_device{}()},
            dist = std::uniform_int_distribution<int>{0, 255}
        ]() mutable -> std::array<unsigned char, 32>
    {
        std::array<unsigned char, 32> arr{};
        for (auto& byte : arr)
        {
            byte = static_cast<unsigned char>(dist(engine));
        }
        return arr;
    };
    while (true)
    {
        auto block = newBlock(pubKey);

        while (getBlockHeaderHash(block.header) > block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {
            if (isMining == false ) return;
            block.header.nonce = generateNonce();
        }

        if (getBlockHeaderHash(block.header) > block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {
            std::cout << "Block mined!\n";
            asio::co_spawn(miningIo, BroadcastNewBlock(miningIo, block), asio::detached);
            miningIo.run();
        }
    }
}