#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <optional>
#include <vector>

namespace mad::network {

struct Address {
    std::string host;
    uint16_t port;
};

// Thin UDP socket wrapper. No reliability layer yet — that comes later.
class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();

    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&& other) noexcept;
    UDPSocket& operator=(UDPSocket&& other) noexcept;

    bool bind(uint16_t port);
    bool send_to(const Address& dest, std::span<const uint8_t> data);

    struct RecvResult {
        Address from;
        std::vector<uint8_t> data;
    };

    // Non-blocking receive. Returns nullopt if no data available.
    std::optional<RecvResult> recv_from();

    void close();
    bool is_valid() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

} // namespace mad::network
