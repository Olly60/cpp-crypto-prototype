void setBlockchainTip(const Array256_t& newTip)
{
    fs::create_directories(paths::blockchainTip.parent_path());

    std::ofstream blockchainTipFile(paths::blockchainTip, std::ios::trunc | std::ios::binary);
    if (!blockchainTipFile)
    {
        throw std::runtime_error("Failed to open blockchain tip file for writing");
    }

    appendToFile(blockchainTipFile, newTip);
}

Array256_t getBlockchainTip()
{
    if (!fs::exists(paths::blockchainTip))
    {
        throw std::runtime_error("Blockchain tip file does not exist");
    }

    std::ifstream file(paths::blockchainTip, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Failed to open blockchain tip file");
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize < static_cast<std::streamsize>(sizeof(Array256_t)))
    {
        throw std::runtime_error("Blockchain tip file is empty or corrupted");
    }

    file.seekg(fileSize - static_cast<std::streamsize>(sizeof(Array256_t)), std::ios::beg);

    Array256_t tip;
    file.read(reinterpret_cast<char*>(tip.data()), sizeof(Array256_t));
    if (!file)
    {
        throw std::runtime_error("Failed to read blockchain tip");
    }

    return tip;
}
