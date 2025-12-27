#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <asio/ip/address.hpp>

#include "crypto_utils.h"



const std::filesystem::path KNOWN_PEERS = "known_peers";
const std::filesystem::path UNKNOWN_PEERS = "unknown_peers";

struct PeerAddress
{
    asio::ip::address address;
    uint16_t port{};

    bool operator==(const PeerAddress& other) const noexcept
    {
        return this->port == other.port && this->address == other.address;
    }

};

struct PeerAddressHash
{
    std::size_t operator()(const PeerAddress& p) const noexcept
    {
        const std::size_t h1 = std::hash<std::string>{}(p.address.to_string());
        const std::size_t h2 = std::hash<uint16_t>{}(p.port);
        return h1 ^ (h2 << 1);
    }
};



struct PeerStatus
{
    uint64_t services{};
    uint64_t lastSeen{};
    uint8_t relay{};
};

using KnownPeersMap = std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash>;

using UnknownPeersMap = std::unordered_set<PeerAddress, PeerAddressHash>;

// Global state
inline KnownPeersMap knownPeers;
inline UnknownPeersMap unknownPeers;

void storePeers();

void loadPeers();