#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cc::neolux::utils::unitransmit {

struct GattConfig {
    std::string local_device;
    std::string remote_device;
    std::string service;
    std::string tx_char;
    std::string rx_char;
    bool blocking = true;
    int timeout_ms = -1;

    static GattConfig from_url(const UrlParts &parts, const Options &opts);
};

class GattTransport final : public ITransport {
public:
    struct EndpointBuffer {
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::uint8_t> bytes;
    };

    explicit GattTransport(const GattConfig &config);
    GattTransport(const UrlParts &parts, const Options &opts);
    ~GattTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    GattConfig config_;
    std::shared_ptr<EndpointBuffer> inbound_;

    bool wait_for_data(std::unique_lock<std::mutex> &lock) const;
};

} // namespace cc::neolux::utils::unitransmit
