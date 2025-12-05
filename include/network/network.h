#pragma once

struct PeerAddressHash {
    std::size_t operator()(const PeerAddress& p) const noexcept {
        std::size_t h1 = std::hash<std::string>{}(p.address);
        std::size_t h2 = std::hash<uint16_t>{}(p.port);
        return h1 ^ (h2 << 1);
    }
};

struct PeerAddress {
    std::string address;
    uint16_t port{};

    bool operator==(const PeerAddress& other) const {
        return address == other.address && port == other.port;
    }

};

struct PeerStatus {
    uint64_t services{};
    uint64_t lastSeen{};
};

