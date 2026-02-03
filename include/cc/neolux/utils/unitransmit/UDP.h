#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"
#include "cc/neolux/utils/unitransmit/Utils.h"

#include <vector>

namespace cc::neolux::utils::unitransmit {

class UdpTransport final : public ITransport {
public:
    UdpTransport(const UrlParts &parts, const Options &opts);
    ~UdpTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    Options opts_;
    SocketHandle sock_ = kInvalidSocket;
    bool has_remote_ = false;
    sockaddr_storage remote_addr_{};
    socklen_t remote_len_ = 0;
    bool has_broadcast_ = false;
    sockaddr_storage broadcast_addr_{};
    socklen_t broadcast_len_ = 0;
    bool has_last_peer_ = false;
    sockaddr_storage last_peer_{};
    socklen_t last_peer_len_ = 0;
    std::vector<std::pair<sockaddr_storage, socklen_t>> peers_;
    std::vector<std::uint8_t> pending_;
    std::size_t pending_offset_ = 0;

    bool add_peer(const sockaddr_storage &addr, socklen_t len);
};

} // namespace cc::neolux::utils::unitransmit
