#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <string>
#include <vector>

namespace cc::neolux::utils::unitransmit {

struct SerialConfig {
    std::string path;
    int baud = 115200;
    int data_bits = 8;
    int stop_bits = 1;
    char parity = 'n';
    bool blocking = true;
    int timeout_ms = -1;

    static SerialConfig from_url(const UrlParts &parts, const Options &opts);
};

class SerialTransport final : public ITransport {
public:
    explicit SerialTransport(const SerialConfig &config);
    SerialTransport(const UrlParts &parts, const Options &opts);
    ~SerialTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    SerialConfig config_;
#ifdef _WIN32
    void close_handle();
    void *handle_ = nullptr;
#else
    int fd_ = -1;
    bool wait_for_fd(int timeout_ms) const;
#endif
};

} // namespace cc::neolux::utils::unitransmit
