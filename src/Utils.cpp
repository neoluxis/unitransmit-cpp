#include "cc/neolux/utils/unitransmit/Utils.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace cc::neolux::utils::unitransmit {

#ifdef _WIN32
namespace {
class WsaInit {
public:
    WsaInit() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WsaInit() { WSACleanup(); }
};
} // namespace
#endif

void ensure_wsa() {
#ifdef _WIN32
    static WsaInit wsa;
#endif
}

void close_socket(SocketHandle sock) {
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
#else
    if (sock >= 0) {
        close(sock);
    }
#endif
}

bool set_socket_nonblocking(SocketHandle sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool wait_for_read(SocketHandle sock, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
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
    int rc = select(sock + 1, &readfds, nullptr, nullptr, ptv);
#endif
    return rc > 0;
}

bool resolve_address(const std::string &host, int port, sockaddr_storage &out, socklen_t &out_len,
                     int socktype) {
    addrinfo hints{};
    hints.ai_socktype = socktype;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    addrinfo *result = nullptr;
    std::string port_str = std::to_string(port);
    const char *host_ptr = host.empty() ? nullptr : host.c_str();
    if (getaddrinfo(host_ptr, port_str.c_str(), &hints, &result) != 0) {
        return false;
    }
    for (auto *ai = result; ai != nullptr; ai = ai->ai_next) {
        if (ai->ai_addrlen <= sizeof(out)) {
            std::memcpy(&out, ai->ai_addr, ai->ai_addrlen);
            out_len = static_cast<socklen_t>(ai->ai_addrlen);
            freeaddrinfo(result);
            return true;
        }
    }
    freeaddrinfo(result);
    return false;
}

std::size_t socket_in_waiting(SocketHandle sock) {
#ifdef _WIN32
    u_long available = 0;
    if (ioctlsocket(sock, FIONREAD, &available) != 0) {
        return 0;
    }
    return static_cast<std::size_t>(available);
#else
    int available = 0;
    if (ioctl(sock, FIONREAD, &available) != 0) {
        return 0;
    }
    return static_cast<std::size_t>(available);
#endif
}

} // namespace cc::neolux::utils::unitransmit
