#include "cc/neolux/utils/unitransmit/TCP.h"

#include "cc/neolux/utils/unitransmit/Utils.h"

#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

namespace cc::neolux::utils::unitransmit {

TcpClientTransport::TcpClientTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    ensure_wsa();
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == kInvalidSocket) {
        return;
    }

    sockaddr_storage addr{};
    socklen_t addr_len = 0;
    if (!resolve_address(parts.host, parts.port, addr, addr_len, SOCK_STREAM)) {
        close_socket(sock_);
        sock_ = kInvalidSocket;
        return;
    }
    if (connect(sock_, reinterpret_cast<sockaddr *>(&addr), addr_len) != 0) {
        close_socket(sock_);
        sock_ = kInvalidSocket;
        return;
    }
    set_socket_nonblocking(sock_);
}

TcpClientTransport::~TcpClientTransport() { close_socket(sock_); }

std::size_t TcpClientTransport::in_waiting() const { return socket_in_waiting(sock_); }

std::vector<std::uint8_t> TcpClientTransport::read() { return read(1); }

std::vector<std::uint8_t> TcpClientTransport::read_all() {
    std::vector<std::uint8_t> out;
    std::size_t available = in_waiting();
    if (available == 0 && opts_.blocking) {
        if (!wait_for_read(sock_, opts_.timeout_ms)) {
            return out;
        }
        available = in_waiting();
    }
    while (available > 0) {
        auto chunk = read(available);
        if (chunk.empty()) {
            break;
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
        available = in_waiting();
    }
    return out;
}

std::vector<std::uint8_t> TcpClientTransport::read(std::size_t max_bytes) {
    if (sock_ == kInvalidSocket || max_bytes == 0) {
        return {};
    }
    if (opts_.blocking) {
        if (!wait_for_read(sock_, opts_.timeout_ms)) {
            return {};
        }
    }
    std::vector<std::uint8_t> buffer(max_bytes);
    int rc = recv(sock_, reinterpret_cast<char *>(buffer.data()), static_cast<int>(buffer.size()), 0);
    if (rc <= 0) {
        return {};
    }
    buffer.resize(static_cast<std::size_t>(rc));
    return buffer;
}

std::size_t TcpClientTransport::write(const std::uint8_t *data, std::size_t size) {
    if (sock_ == kInvalidSocket || !data || size == 0) {
        return 0;
    }
    int rc = send(sock_, reinterpret_cast<const char *>(data), static_cast<int>(size), 0);
    if (rc <= 0) {
        return 0;
    }
    return static_cast<std::size_t>(rc);
}

TcpServerTransport::TcpServerTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    ensure_wsa();
    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == kInvalidSocket) {
        return;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse),
               sizeof(reuse));
#else
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_storage addr{};
    socklen_t addr_len = 0;
    if (!resolve_address(parts.host, parts.port, addr, addr_len, SOCK_STREAM)) {
        close_socket(listen_sock_);
        listen_sock_ = kInvalidSocket;
        return;
    }
    if (bind(listen_sock_, reinterpret_cast<sockaddr *>(&addr), addr_len) != 0) {
        close_socket(listen_sock_);
        listen_sock_ = kInvalidSocket;
        return;
    }
    if (listen(listen_sock_, 8) != 0) {
        close_socket(listen_sock_);
        listen_sock_ = kInvalidSocket;
        return;
    }
    set_socket_nonblocking(listen_sock_);
}

TcpServerTransport::~TcpServerTransport() {
    for (auto sock : clients_) {
        close_socket(sock);
    }
    close_socket(listen_sock_);
}

std::size_t TcpServerTransport::in_waiting() const {
    for (auto sock : clients_) {
        auto available = socket_in_waiting(sock);
        if (available > 0) {
            return available;
        }
    }
    return 0;
}

std::vector<std::uint8_t> TcpServerTransport::read() { return read(1); }

std::vector<std::uint8_t> TcpServerTransport::read_all() {
    std::vector<std::uint8_t> out;
    std::size_t available = in_waiting();
    if (available == 0 && opts_.blocking) {
        if (!wait_for_any_client(opts_.timeout_ms)) {
            return out;
        }
        available = in_waiting();
    }
    while (available > 0) {
        auto chunk = read(available);
        if (chunk.empty()) {
            break;
        }
        out.insert(out.end(), chunk.begin(), chunk.end());
        available = in_waiting();
    }
    return out;
}

std::vector<std::uint8_t> TcpServerTransport::read(std::size_t max_bytes) {
    if (max_bytes == 0) {
        return {};
    }
    accept_clients();
    if (clients_.empty()) {
        if (opts_.blocking) {
            if (!accept_blocking()) {
                return {};
            }
        } else {
            return {};
        }
    }

    if (opts_.blocking && !wait_for_any_client(opts_.timeout_ms)) {
        return {};
    }

    for (auto sock : clients_) {
        if (sock == kInvalidSocket) {
            continue;
        }
        std::vector<std::uint8_t> buffer(max_bytes);
        int rc = recv(sock, reinterpret_cast<char *>(buffer.data()), static_cast<int>(buffer.size()), 0);
        if (rc > 0) {
            buffer.resize(static_cast<std::size_t>(rc));
            return buffer;
        }
    }
    return {};
}

std::size_t TcpServerTransport::write(const std::uint8_t *data, std::size_t size) {
    if (!data || size == 0) {
        return 0;
    }
    accept_clients();
    if (clients_.empty()) {
        return 0;
    }
    std::size_t total = 0;
    int remaining = opts_.max_connect;
    for (auto sock : clients_) {
        if (sock == kInvalidSocket) {
            continue;
        }
        int rc = send(sock, reinterpret_cast<const char *>(data), static_cast<int>(size), 0);
        if (rc > 0) {
            total = static_cast<std::size_t>(rc);
            if (opts_.max_connect > 0) {
                --remaining;
                if (remaining <= 0) {
                    break;
                }
            }
        }
    }
    return total;
}

void TcpServerTransport::accept_clients() {
    if (listen_sock_ == kInvalidSocket) {
        return;
    }
    for (;;) {
        sockaddr_storage addr{};
        socklen_t addr_len = sizeof(addr);
        SocketHandle client = accept(listen_sock_, reinterpret_cast<sockaddr *>(&addr), &addr_len);
        if (client == kInvalidSocket) {
            break;
        }
        set_socket_nonblocking(client);
        clients_.push_back(client);
        if (opts_.max_connect > 0 && static_cast<int>(clients_.size()) >= opts_.max_connect) {
            break;
        }
    }
}

bool TcpServerTransport::accept_blocking() {
    if (listen_sock_ == kInvalidSocket) {
        return false;
    }
    if (!wait_for_read(listen_sock_, opts_.timeout_ms)) {
        return false;
    }
    accept_clients();
    return !clients_.empty();
}

bool TcpServerTransport::wait_for_any_client(int timeout_ms) {
    if (clients_.empty()) {
        return false;
    }
    fd_set readfds;
    FD_ZERO(&readfds);
    SocketHandle max_fd = 0;
    for (auto sock : clients_) {
        FD_SET(sock, &readfds);
        if (sock > max_fd) {
            max_fd = sock;
        }
    }
    timeval tv{};
    timeval *ptv = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }
#ifdef _WIN32
    int rc = select(0, &readfds, nullptr, nullptr, ptv);
#else
    int rc = select(max_fd + 1, &readfds, nullptr, nullptr, ptv);
#endif
    return rc > 0;
}

} // namespace cc::neolux::utils::unitransmit
