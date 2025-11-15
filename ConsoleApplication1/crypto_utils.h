#pragma once

void bytesFromHex(const std::string& hex, array256_t& out);

void hexFromBytes(const array256_t& bytes, const uint64_t& size, std::string& out);

void sha256Of(const void* data, const uint64_t& len, array256_t& out);
