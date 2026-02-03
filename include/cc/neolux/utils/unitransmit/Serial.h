#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <vector>

namespace cc::neolux::utils::unitransmit {

class SerialTransport final : public ITransport {
public:
    SerialTransport(const UrlParts &parts, const Options &opts);
    ~SerialTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;

private:
    Options opts_;
#ifdef _WIN32
    void close_handle();
    void *handle_ = nullptr;
#else
    int fd_ = -1;
    bool wait_for_fd(int timeout_ms) const;
#endif
};

} // namespace cc::neolux::utils::unitransmit
