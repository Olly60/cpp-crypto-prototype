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
#include "storage/block/block_indexes.h"
#include <boost/multiprecision/number.hpp>
#include <boost/multiprecision/cpp_int.hpp>

Array256_t getBlockWork(const Array256_t& target) {

    boost::multiprecision::uint256_t resultNum;
    boost::multiprecision::import_bits(resultNum, target.rbegin(), target.rend(), 8, true);

    boost::multiprecision::uint256_t max = ~boost::multiprecision::uint256_t{0};
    resultNum = max / resultNum;

    Array256_t resultArr{};
    boost::multiprecision::export_bits(resultNum, resultArr.rbegin(), 8, true);
    return resultArr;
}

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b)
{

    boost::multiprecision::uint256_t aNum;
    boost::multiprecision::uint256_t bNum;
    boost::multiprecision::import_bits(aNum, a.rbegin(), a.rend(), 8, true);
    boost::multiprecision::import_bits(bNum, b.rbegin(), b.rend(), 8, true);

    boost::multiprecision::uint256_t resultNum = aNum + bNum;

    Array256_t resultArr{};
    boost::multiprecision::export_bits(resultNum, resultArr.rbegin(), 8, true);
    return resultArr;
}

// Shift right (harder)
Array256_t shiftRight(const Array256_t& arr)
{
    boost::multiprecision::uint256_t resultNum;
    boost::multiprecision::import_bits(resultNum, arr.rbegin(), arr.rend(), 8, true);

    resultNum = resultNum >> 1;

    Array256_t resultArr{};
    boost::multiprecision::export_bits(resultNum, resultArr.rbegin(), 8, true);
    return resultArr;
}

// Shift left (easier)
Array256_t shiftLeft(const Array256_t& arr)
{
    boost::multiprecision::uint256_t resultNum;
    boost::multiprecision::import_bits(resultNum, arr.rbegin(), arr.rend(), 8, true);

    resultNum = resultNum << 1;

    Array256_t resultArr{};
    boost::multiprecision::export_bits(resultNum, resultArr.rbegin(), 8, true);
    return resultArr;
}

ChainBlock newBlock(Array256_t pubKey)
{
    auto currentTip = getTipHash();
    ChainBlock block;
    block.header.prevBlockHash = getTipHash();
    block.header.timestamp = getCurrentTimestamp();

    // Difficulty adjustment (target-based)
    auto currentTipHeader = getBlockHeader(currentTip);

    uint64_t prevTimestamp = 0;

    if (tryGetBlockIndex(getTipHash())->height > 0)
    {
        prevTimestamp = getBlockHeader(currentTipHeader->prevBlockHash)->timestamp;
    }

    uint64_t timeDelta =
        (currentTipHeader->timestamp > prevTimestamp)
            ? currentTipHeader->timestamp - prevTimestamp
            : 0;

    if (timeDelta < 600)
    {
        block.header.difficulty = shiftLeft(currentTipHeader->difficulty);
    }
    else
    {
        block.header.difficulty = shiftLeft(currentTipHeader->difficulty);
    }

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

asio::io_context miningIo;

void mineBlocks(Array256_t pubKey)
{
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
        BytesBuffer buf;
        buf.writeArray256(block.header.difficulty);
        std::cout << "Current difficulty: " << bytesToHex(buf) << "\n";
        while (getBlockHeaderHash(block.header) > block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {
            if (isMining == false ) return;
            block.header.nonce = generateNonce();

        }

        if (getBlockHeaderHash(block.header) < block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {
            std::cout << "Block mined!\n";
            std::cout << "Adding block to chain...\n";
            addNewTipBlock(block);
            asio::co_spawn(ioCtx, BroadcastNewBlock(miningIo, block), asio::detached);
            std::cout << "Broadcasting block to peers...\n";
        }

    }
}