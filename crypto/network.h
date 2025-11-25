#pragma once

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

