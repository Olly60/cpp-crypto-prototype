#pragma once

struct PeerStatus {
    uint64_t services{};
    uint64_t lastSeen{};
};

struct PeerAddress {
    std::string address;
    uint16_t port{};
};