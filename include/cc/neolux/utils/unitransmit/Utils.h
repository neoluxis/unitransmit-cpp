#pragma once

#include <cstddef>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace cc::neolux::utils::unitransmit {

#ifdef _WIN32
using SocketHandle = SOCKET;
const SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
const SocketHandle kInvalidSocket = -1;
#endif

void ensure_wsa();
void close_socket(SocketHandle sock);
bool set_socket_nonblocking(SocketHandle sock);
bool wait_for_read(SocketHandle sock, int timeout_ms);
bool resolve_address(const std::string &host, int port, sockaddr_storage &out, socklen_t &out_len,
                     int socktype);
std::size_t socket_in_waiting(SocketHandle sock);

} // namespace cc::neolux::utils::unitransmit
