#include "transaction.h"
#include <sodium/crypto_sign.h>
#include "crypto_utils.h"
#include "storage/utxo_storage.h"

Array256_t getTxHash(const Tx& tx)
{
    return sha256Of(serialiseTx(tx));
}

Array256_t getMerkleRoot(const std::vector<Tx>& txs)
{
    if (txs.empty())
    {
        return Array256_t{}; // Empty merkle root for no transactions
    }

    // Build initial layer from transaction hashes
    std::vector<Array256_t> currentLayer;
    currentLayer.reserve(txs.size());
    for (const auto& tx : txs)
    {
        currentLayer.push_back(getTxHash(tx));
    }

    // Build merkle tree bottom-up
    while (currentLayer.size() > 1)
    {
        std::vector<Array256_t> nextLayer;
        nextLayer.reserve((currentLayer.size() + 1) / 2);

        for (size_t i = 0; i < currentLayer.size(); i += 2)
        {
            const Array256_t& left = currentLayer[i];
            // If odd number of elements, duplicate the last one
            const Array256_t& right = (i + 1 < currentLayer.size())
                                          ? currentLayer[i + 1]
                                          : currentLayer[i];

            // Hash the concatenation of left and right
            BytesBuffer combined;
            combined.reserve(left.size() + right.size());
            combined.writeArray256(left);
            combined.writeArray256(right);

            nextLayer.push_back(sha256Of(combined));
        }

        currentLayer = std::move(nextLayer);
    }

    return currentLayer[0];
}



Array256_t computeTxSignHash(const Tx& tx, uint64_t inputIndex)
{
    BytesBuffer buf;
    buf.reserve(tx.txInputs.size() * 40 + tx.txOutputs.size() * 40 + 16);

    buf.writeU64(tx.version);

    // Inputs
    buf.writeU64(tx.txInputs.size());
    for (size_t i = 0; i < tx.txInputs.size(); ++i)
    {
        const auto& input = tx.txInputs[i];
        buf.writeArray256(input.utxoId.UTXOTxHash);
        buf.writeU64(input.utxoId.UTXOOutputIndex);

        // Add the input index only for the input being signed
        if (i == inputIndex)
            buf.writeU64(i);
    }

    // Outputs
    buf.writeU64(tx.txOutputs.size());
    for (const auto& [amount, recipient] : tx.txOutputs)
    {
        buf.writeU64(amount);
        buf.writeArray256(recipient);
    }

    return sha256Of(buf);
}

Tx signTxInputs(const Tx& tx, const Array512_t& sk) // full secret key
{
    Tx signedTx = tx; // copy

    for (size_t i = 0; i < signedTx.txInputs.size(); i++)
    {
        Array256_t hash = computeTxSignHash(signedTx, i); // input-specific hash

        Array512_t sig;
        crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(), sk.data());

        signedTx.txInputs[i].signature = sig;
    }

    return signedTx;
}



BytesBuffer serialiseTx(const Tx& tx)
{
    BytesBuffer txBytes;

    // Version
    txBytes.writeU64(tx.version);

    // Inputs amount
    txBytes.writeU64(tx.txInputs.size());

    // Inputs
    for (const auto& txInput : tx.txInputs)
    {
        txBytes.writeArray256(txInput.utxoId.UTXOTxHash);
        txBytes.writeU64(txInput.utxoId.UTXOOutputIndex);
        txBytes.writeArray512(txInput.signature);
    }

    // Outputs amount
    txBytes.writeU64(tx.txOutputs.size());

    // Outputs
    for (const auto& txOutput : tx.txOutputs)
    {
        txBytes.writeU64(txOutput.amount);
        txBytes.writeArray256(txOutput.recipient);
    }

    return txBytes;
}

Tx parseTx(BytesBuffer& txBytes)
{
    Tx tx;

    // Tx Version
    tx.version = txBytes.readU64();

    // Input amount
    uint64_t inputAmount = txBytes.readU64();
    tx.txInputs.reserve(inputAmount);

    // Read inputs
    for (uint64_t i = 0; i < inputAmount; i++)
    {
        TxInput txInput;
        txInput.utxoId.UTXOTxHash = txBytes.readArray256();
        txInput.utxoId.UTXOOutputIndex = txBytes.readU64();
        txInput.signature = txBytes.readArray512();
        tx.txInputs.push_back(txInput);
    }

    // Output amount
    uint64_t outputAmount = txBytes.readU64();
    tx.txOutputs.reserve(outputAmount);

    // Read outputs
    for (uint64_t i = 0; i < outputAmount; i++)
    {
        TxOutput txOutput;
        txOutput.amount = txBytes.readU64();
        txOutput.recipient = txBytes.readArray256();
        tx.txOutputs.push_back(txOutput);
    }

    return tx;
}

Tx makeTx(const std::unordered_set<UTXOId, UTXOIdHash>& utxos, const Array512_t& secKey, const Array256_t& recipient, uint64_t amount)
{
    Tx tx;
    tx.version = 1;

    // Spend Utxos
    for (const auto& utxo : utxos)
    {
        TxInput txInput;
        txInput.utxoId = utxo;
        tx.txInputs.push_back(txInput);
    }

    uint64_t inputAmount = 0;
    for (const auto& utxo : utxos)
    {
        inputAmount += tryGetUtxo(utxo)->amount;
    }

    if (inputAmount < amount + std::max(inputAmount / 100, static_cast<uint64_t>(1))) throw std::runtime_error("Not enough funds");

    tx.txOutputs.push_back({amount, recipient});

    return signTxInputs(tx, secKey);
}



