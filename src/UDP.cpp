#include "cc/neolux/utils/unitransmit/UDP.h"

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Utils.h"

#include <algorithm>
#include <cstring>

namespace cc::neolux::utils::unitransmit {

namespace {
bool sockaddr_equal(const sockaddr_storage &a, socklen_t a_len, const sockaddr_storage &b,
                    socklen_t b_len) {
    if (a_len != b_len) {
        return false;
    }
    return std::memcmp(&a, &b, static_cast<std::size_t>(a_len)) == 0;
}
} // namespace

UdpTransport::UdpTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    ensure_wsa();
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == kInvalidSocket) {
        return;
    }

    if (parts.port > 0 || !parts.host.empty()) {
        sockaddr_storage addr{};
        socklen_t addr_len = 0;
        if (resolve_address(parts.host, parts.port, addr, addr_len, SOCK_DGRAM)) {
            bind(sock_, reinterpret_cast<sockaddr *>(&addr), addr_len);
        }
    }

    if (!opts_.remote.empty()) {
        auto remote = parse_endpoint(opts_.remote);
        if (remote) {
            if (resolve_address(remote->first, remote->second, remote_addr_, remote_len_, SOCK_DGRAM)) {
                has_remote_ = true;
            }
        }
    }

    if (opts_.broadcast) {
        int enable = 1;
#ifdef _WIN32
        setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&enable),
                   sizeof(enable));
#else
        setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
#endif
        if (parts.port > 0) {
            if (resolve_address("255.255.255.255", parts.port, broadcast_addr_, broadcast_len_,
                                SOCK_DGRAM)) {
                has_broadcast_ = true;
            }
        }
    }

    set_socket_nonblocking(sock_);
}

UdpTransport::~UdpTransport() { close_socket(sock_); }

std::size_t UdpTransport::in_waiting() const {
    std::size_t pending = 0;
    if (pending_offset_ < pending_.size()) {
        pending = pending_.size() - pending_offset_;
    }
    if (pending > 0) {
        return pending + socket_in_waiting(sock_);
    }
    return socket_in_waiting(sock_);
}

std::vector<std::uint8_t> UdpTransport::read() { return read(1); }

std::vector<std::uint8_t> UdpTransport::read_all() {
    std::vector<std::uint8_t> out;
    if (pending_offset_ < pending_.size()) {
        out.insert(out.end(), pending_.begin() + static_cast<std::ptrdiff_t>(pending_offset_),
                   pending_.end());
        pending_.clear();
        pending_offset_ = 0;
    }
    std::size_t available = socket_in_waiting(sock_);
    while (available > 0) {
        auto chunk = read(available);
        if (chunk.empty()) {
            break;
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
        available = socket_in_waiting(sock_);
    }
    return out;
}

std::vector<std::uint8_t> UdpTransport::read(std::size_t max_bytes) {
    if (sock_ == kInvalidSocket || max_bytes == 0) {
        return {};
    }
    if (pending_offset_ < pending_.size()) {
        const std::size_t remaining = pending_.size() - pending_offset_;
        const std::size_t count = std::min(max_bytes, remaining);
        std::vector<std::uint8_t> out(pending_.begin() + static_cast<std::ptrdiff_t>(pending_offset_),
                                      pending_.begin() +
                                          static_cast<std::ptrdiff_t>(pending_offset_ + count));
        pending_offset_ += count;
        if (pending_offset_ >= pending_.size()) {
            pending_.clear();
            pending_offset_ = 0;
        }
        return out;
    }
    if (opts_.blocking) {
        if (!wait_for_read(sock_, opts_.timeout_ms)) {
            return {};
        }
    }
    std::size_t available = socket_in_waiting(sock_);
    if (available == 0) {
        return {};
    }
    std::vector<std::uint8_t> buffer(available);
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);
    int rc = recvfrom(sock_, reinterpret_cast<char *>(buffer.data()),
                      static_cast<int>(buffer.size()), 0,
                      reinterpret_cast<sockaddr *>(&from), &from_len);
    if (rc <= 0) {
        return {};
    }
    last_peer_ = from;
    last_peer_len_ = from_len;
    has_last_peer_ = true;
    if (opts_.max_connect != 1) {
        add_peer(from, from_len);
    }
    buffer.resize(static_cast<std::size_t>(rc));
    if (buffer.size() <= max_bytes) {
        return buffer;
    }
    std::vector<std::uint8_t> out(buffer.begin(),
                                  buffer.begin() + static_cast<std::ptrdiff_t>(max_bytes));
    pending_.assign(buffer.begin() + static_cast<std::ptrdiff_t>(max_bytes), buffer.end());
    pending_offset_ = 0;
    return out;
}

std::size_t UdpTransport::write(const std::uint8_t *data, std::size_t size) {
    if (sock_ == kInvalidSocket || !data || size == 0) {
        return 0;
    }
    if (has_remote_) {
        int rc = sendto(sock_, reinterpret_cast<const char *>(data), static_cast<int>(size), 0,
                        reinterpret_cast<sockaddr *>(&remote_addr_), remote_len_);
        if (rc <= 0) {
            return 0;
        }
        return static_cast<std::size_t>(rc);
    }
    if (has_broadcast_) {
        int rc = sendto(sock_, reinterpret_cast<const char *>(data), static_cast<int>(size), 0,
                        reinterpret_cast<sockaddr *>(&broadcast_addr_), broadcast_len_);
        if (rc <= 0) {
            return 0;
        }
        return static_cast<std::size_t>(rc);
    }
    if (opts_.max_connect != 1) {
        std::size_t sent_any = 0;
        int remaining = opts_.max_connect;
        for (const auto &peer : peers_) {
            int rc = sendto(sock_, reinterpret_cast<const char *>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr *>(&peer.first), peer.second);
            if (rc > 0) {
                sent_any = static_cast<std::size_t>(rc);
                if (opts_.max_connect > 0) {
                    --remaining;
                    if (remaining <= 0) {
                        break;
                    }
                }
            }
        }
        return sent_any;
    }
    if (has_last_peer_) {
        int rc = sendto(sock_, reinterpret_cast<const char *>(data), static_cast<int>(size), 0,
                        reinterpret_cast<sockaddr *>(&last_peer_), last_peer_len_);
        if (rc <= 0) {
            return 0;
        }
        return static_cast<std::size_t>(rc);
    }
    return 0;
}

bool UdpTransport::add_peer(const sockaddr_storage &addr, socklen_t len) {
    for (const auto &peer : peers_) {
        if (sockaddr_equal(peer.first, peer.second, addr, len)) {
            return false;
        }
    }
    peers_.push_back({addr, len});
    if (opts_.max_connect > 0 && static_cast<int>(peers_.size()) > opts_.max_connect) {
        peers_.erase(peers_.begin());
    }
    return true;
}

} // namespace cc::neolux::utils::unitransmit
