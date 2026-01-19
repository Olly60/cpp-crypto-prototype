#include "block_work.h"
#include <iostream>
#include <ranges>
#include <thread>
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

#include "verify.h"

Array256_t uint256ToArray(const boost::multiprecision::uint256_t& num)
{
    Array256_t result{};
    std::vector<uint8_t> tmp;

    boost::multiprecision::export_bits(
        num, std::back_inserter(tmp), 8, true
    );

    const size_t offset = 32 - tmp.size();
    for (size_t i = 0; i < tmp.size(); ++i)
        result[offset + i] = tmp[i];
    return result;
}


Array256_t getBlockWork(const Array256_t& target)
{
    using boost::multiprecision::uint256_t;

    uint256_t targetNum;
    boost::multiprecision::import_bits(
        targetNum, target.begin(), target.end(), 8, true
    );

    uint256_t max = ~uint256_t{0};
    uint256_t work = (targetNum == 0) ? max : (max / targetNum);

    return uint256ToArray(work);
}

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b)
{
    boost::multiprecision::uint256_t aNum;
    boost::multiprecision::uint256_t bNum;

    boost::multiprecision::import_bits(aNum, a.begin(), a.end(), 8, true);
    boost::multiprecision::import_bits(bNum, b.begin(), b.end(), 8, true);

    boost::multiprecision::uint256_t resultNum = aNum + bNum;

    return uint256ToArray(resultNum);
}

// Shift right (harder)
Array256_t shiftRight(const Array256_t& arr)
{
    boost::multiprecision::uint256_t resultNum;
    boost::multiprecision::import_bits(resultNum, arr.begin(), arr.end(), 8, true);

    resultNum >>= 1;

    return uint256ToArray(resultNum);
}

// Shift left (easier)
Array256_t shiftLeft(const Array256_t& arr)
{
    boost::multiprecision::uint256_t resultNum;
    boost::multiprecision::import_bits(resultNum, arr.begin(), arr.end(), 8, true);

    resultNum <<= 1;

    return uint256ToArray(resultNum);
}

ChainBlock newBlock(Array256_t pubKey)
{
    auto currentTipHash = getTipHash();
    ChainBlock block;
    block.header.prevBlockHash = currentTipHash;

    // Difficulty
    auto currentTipHeader = getBlockHeader(currentTipHash);
    uint64_t prevPrevTimestamp = tryGetBlockIndex(getTipHash())->height > 0
                                     ? getBlockHeader(currentTipHeader->prevBlockHash)->timestamp
                                     : currentTipHeader->timestamp;

    uint64_t timeDelta = currentTipHeader->timestamp - prevPrevTimestamp;

    block.header.difficulty = timeDelta < 600
                                  ? shiftRight(currentTipHeader->difficulty)
                                  : shiftLeft(currentTipHeader->difficulty);

    std::unordered_set<UTXOId, UTXOIdHash> utxosInBlock;

    // Transactions
    uint64_t size = 0;
    for (auto& val : mempool | std::views::values)
    {
        size += serialiseTx(val).size();
        bool used = false;
        for (const auto& txInput : val.txInputs)
        {
            if (!utxosInBlock.insert(txInput.utxoId).second) used = true;
        }
        if (used) continue;

        if (size > MAX_BLOCK_SIZE - calculateBlockHeaderSize()) break;
        block.txs.push_back(val);
    }

    uint64_t totalFees = 0;
    for (auto& tx : block.txs)
    {
        uint64_t totalInputAmount = 0;
        for (const auto& input : tx.txInputs)
        {
            auto utxoInDb = tryGetUtxo(input.utxoId);
            totalInputAmount += utxoInDb->amount;
        }

        uint64_t txFee = std::max(totalInputAmount / 100, static_cast<uint64_t>(1));
        totalFees += txFee;
    }

    // Coinbase
    Tx coinbaseTx;
    coinbaseTx.txOutputs.push_back({totalFees + BLOCK_REWARD, pubKey});

    BytesBuffer nonceBuf;
    nonceBuf.writeU64(tryGetBlockIndex(currentTipHash)->height + 1);
    coinbaseTx.nonce = sha256Of(nonceBuf);

    block.txs.insert(block.txs.begin(), coinbaseTx);

    block.header.merkleRoot = getMerkleRoot(block.txs);
    return block;
}

void mineBlocks(const std::stop_token& st, const Array256_t& pubKey)
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

        while (getBlockHeaderHash(block.header) > block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {
            if (st.stop_requested()) return;
            block.header.nonce = generateNonce();
        }

        if (getBlockHeaderHash(block.header) <= block.header.difficulty && block.header.prevBlockHash == getTipHash())
        {

            std::cout << (verifyBlock(block) ? "Block mined!\n" : "Invalid block mined");

            addNewTipBlock(block);
            asio::co_spawn(ioCtx, broadcastNewBlock(ioCtx, block), asio::detached);
        }
    }
}
