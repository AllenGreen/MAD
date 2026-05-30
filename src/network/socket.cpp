#include "network/socket.hpp"
#include "core/log.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <utility>

namespace mad::network {

static constexpr const char* TAG = "Socket";

UDPSocket::UDPSocket() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        core::log::error(TAG, "Failed to create socket: {}", strerror(errno));
        return;
    }

    // Set non-blocking
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

UDPSocket::~UDPSocket() {
    close();
}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)) {}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

bool UDPSocket::bind(uint16_t port) {
    if (!is_valid()) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        core::log::error(TAG, "Bind to port {} failed: {}", port, strerror(errno));
        return false;
    }

    core::log::info(TAG, "Bound to port {}", port);
    return true;
}

bool UDPSocket::send_to(const Address& dest, std::span<const uint8_t> data) {
    if (!is_valid()) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest.port);
    inet_pton(AF_INET, dest.host.c_str(), &addr.sin_addr);

    auto sent = ::sendto(fd_, data.data(), data.size(), 0,
        reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent >= 0;
}

std::optional<UDPSocket::RecvResult> UDPSocket::recv_from() {
    if (!is_valid()) return std::nullopt;

    uint8_t buf[65536];
    sockaddr_in from_addr{};
    socklen_t from_len = sizeof(from_addr);

    auto received = ::recvfrom(fd_, buf, sizeof(buf), 0,
        reinterpret_cast<sockaddr*>(&from_addr), &from_len);

    if (received <= 0) return std::nullopt;

    char host_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from_addr.sin_addr, host_buf, sizeof(host_buf));

    return RecvResult{
        .from = Address{host_buf, ntohs(from_addr.sin_port)},
        .data = std::vector<uint8_t>(buf, buf + received)
    };
}

void UDPSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace mad::network
