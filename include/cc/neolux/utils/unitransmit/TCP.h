#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"
#include "cc/neolux/utils/unitransmit/Utils.h"

#include <string>
#include <vector>

namespace cc::neolux::utils::unitransmit {

struct TcpClientConfig {
    std::string host;
    int port = 0;
    bool blocking = true;
    int timeout_ms = -1;

    static TcpClientConfig from_url(const UrlParts &parts, const Options &opts);
};

struct TcpServerConfig {
    std::string host;
    int port = 0;
    bool blocking = true;
    int timeout_ms = -1;
    int max_connect = 1;

    static TcpServerConfig from_url(const UrlParts &parts, const Options &opts);
};

class TcpClientTransport final : public ITransport {
public:
    explicit TcpClientTransport(const TcpClientConfig &config);
    TcpClientTransport(const UrlParts &parts, const Options &opts);
    ~TcpClientTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    TcpClientConfig config_;
    SocketHandle sock_ = kInvalidSocket;
};

class TcpServerTransport final : public ITransport {
public:
    explicit TcpServerTransport(const TcpServerConfig &config);
    TcpServerTransport(const UrlParts &parts, const Options &opts);
    ~TcpServerTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    void accept_clients();
    bool accept_blocking();
    bool wait_for_any_client(int timeout_ms);

    TcpServerConfig config_;
    SocketHandle listen_sock_ = kInvalidSocket;
    std::vector<SocketHandle> clients_;
};

} // namespace cc::neolux::utils::unitransmit
