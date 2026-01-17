#include "wallet.h"
#include "storage/storage_utils.h"

const std::filesystem::path walletsFilePath = "wallets.dat";

void storeWallets()
{
    BytesBuffer walletFileBytes;
    walletFileBytes.writeU64(wallets.size());
    for (auto& wallet : wallets)
    {
        walletFileBytes.writeArray256(wallet.first);
        walletFileBytes.writeU64(wallet.second.size());
        for (const auto& utxo : wallet.second)
        {
            walletFileBytes.writeArray256(utxo.UTXOTxHash);
            walletFileBytes.writeU64(utxo.UTXOOutputIndex);
        }
    }
    writeFileTrunc(walletsFilePath, walletFileBytes);
}

void loadWallets()
{
    auto walletBytes = readFile(walletsFilePath);
    if (!walletBytes) return;

    uint64_t walletCount = walletBytes->readU64();
    for (uint64_t i = 0; i < walletCount; i++)
    {
        Array256_t walletPubKey = walletBytes->readArray256();
        uint64_t utxoCount = walletBytes->readU64();
        for (uint64_t j = 0; j < utxoCount; j++)
        {
            Array256_t utxoTxHash = walletBytes->readArray256();
            uint64_t utxoOutputIndex = walletBytes->readU64();
            wallets[walletPubKey].insert({ utxoTxHash, utxoOutputIndex });
        }
    }

}
