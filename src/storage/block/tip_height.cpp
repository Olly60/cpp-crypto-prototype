

void addBlockHeight() {
    fs::create_directories(paths::blockHeight.parent_path());

    std::ofstream heightFile(paths::blockHeight, std::ios::trunc | std::ios::binary);
    if (!heightFile) {
        throw std::runtime_error("Failed to open block height file for writing");
    }
    uint64_t height = 0;
    takeBytesInto(height, readWholeFile(paths::blockHeight));
    heightFile.exceptions(std::ios::failbit | std::ios::badbit);
    appendToFile(heightFile, height + 1);

}

void subtractBlockHeight() {
    fs::create_directories(paths::blockHeight.parent_path());

    std::ofstream heightFile(paths::blockHeight, std::ios::trunc | std::ios::binary);
    readWholeFile(paths::blockHeight);
    uint64_t height = 0;
    takeBytesInto(height, readWholeFile(paths::blockHeight));
    if (!heightFile) {
        throw std::runtime_error("Failed to open peers block height for writing");
    }
    heightFile.exceptions(std::ios::failbit | std::ios::badbit);
    if (!height - 1 > height) {
        appendToFile(heightFile, height - 1);
    }
    else {
        throw std::runtime_error("Block height is already 0");
    }

}

uint64_t getBlockHeight() {
    uint64_t height = 0;
    takeBytesInto(height, readWholeFile(paths::blockHeight));
    return height;
}