#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include "cc/neolux/utils/unitransmit/GATT.h"
#include "cc/neolux/utils/unitransmit/MQTT.h"
#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Serial.h"
#include "cc/neolux/utils/unitransmit/TCP.h"
#include "cc/neolux/utils/unitransmit/UDP.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cc::neolux::utils::unitransmit {
namespace {

class LoopbackTransport final : public ITransport {
public:
    explicit LoopbackTransport(std::string label) : label_(std::move(label)) {}

    std::size_t in_waiting() const override { return buffer_.size(); }

    std::vector<std::uint8_t> read() override {
        if (buffer_.empty()) {
            return {};
        }
        std::vector<std::uint8_t> out(1);
        out[0] = buffer_.front();
        buffer_.erase(buffer_.begin());
        return out;
    }

    std::vector<std::uint8_t> read_all() override { return read(buffer_.size()); }

    std::vector<std::uint8_t> read(std::size_t max_bytes) override {
        const std::size_t count = std::min(max_bytes, buffer_.size());
        std::vector<std::uint8_t> out;
        out.reserve(count);
        out.insert(out.end(), buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(count));
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(count));
        return out;
    }

    std::size_t write(const std::uint8_t *data, std::size_t size) override {
        if (!data || size == 0) {
            return 0;
        }
        buffer_.insert(buffer_.end(), data, data + size);
        return size;
    }

private:
    std::string label_;
    std::vector<std::uint8_t> buffer_;
};

} // namespace

std::unique_ptr<ITransport> make_transport(const UrlParts &parts, const Options &opts) {
    if (parts.scheme == "loop" || parts.scheme == "loopback") {
        return std::make_unique<LoopbackTransport>(parts.scheme);
    }
    if (parts.scheme == "udp") {
        return std::make_unique<UdpTransport>(UdpConfig::from_url(parts, opts));
    }
    if (parts.scheme == "tcp") {
        return std::make_unique<TcpClientTransport>(TcpClientConfig::from_url(parts, opts));
    }
    if (parts.scheme == "tcp-s") {
        return std::make_unique<TcpServerTransport>(TcpServerConfig::from_url(parts, opts));
    }
    if (parts.scheme == "serial") {
        return std::make_unique<SerialTransport>(SerialConfig::from_url(parts, opts));
    }
    if (parts.scheme == "gatt") {
        return std::make_unique<GattTransport>(GattConfig::from_url(parts, opts));
    }
    if (parts.scheme == "mqtt") {
        return std::make_unique<MqttTransport>(MqttConfig::from_url(parts, opts));
    }
    return std::make_unique<LoopbackTransport>(parts.scheme);
}

std::size_t ITransport::write(const std::vector<std::uint8_t> &data) {
    return write(data.data(), data.size());
}

bool ITransport::is_ready() const { return true; }

std::string ITransport::last_error() const { return {}; }

std::size_t ITransport::write(const std::string &data) {
    return write(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
}

UniTransmit::UniTransmit(std::string ifname) : ifname_(std::move(ifname)) {
    start();
}

UniTransmit::~UniTransmit() { close(); }

std::vector<std::uint8_t> UniTransmit::read() {
    return transport_ ? transport_->read() : std::vector<std::uint8_t>{};
}

std::vector<std::uint8_t> UniTransmit::read_all() {
    return transport_ ? transport_->read_all() : std::vector<std::uint8_t>{};
}

std::vector<std::uint8_t> UniTransmit::read(std::size_t max_bytes) {
    return transport_ ? transport_->read(max_bytes) : std::vector<std::uint8_t>{};
}

std::size_t UniTransmit::write(const std::vector<std::uint8_t> &data) {
    return transport_ ? transport_->write(data) : 0;
}

std::size_t UniTransmit::write(const std::string &data) {
    return transport_ ? transport_->write(data) : 0;
}

std::size_t UniTransmit::in_waiting() const {
    return transport_ ? transport_->in_waiting() : 0;
}

UniTransmit &UniTransmit::operator<<(const std::string &data) {
    write(data);
    return *this;
}

UniTransmit &UniTransmit::operator<<(const std::vector<std::uint8_t> &data) {
    write(data);
    return *this;
}

UniTransmit &UniTransmit::operator>>(std::string &out) {
    const auto data = read_all();
    out.assign(reinterpret_cast<const char *>(data.data()), data.size());
    return *this;
}

UniTransmit &UniTransmit::operator>>(std::vector<std::uint8_t> &out) {
    out = read_all();
    return *this;
}

const std::string &UniTransmit::ifname() const { return ifname_; }

const std::string &UniTransmit::scheme() const { return scheme_; }

bool UniTransmit::is_ready() const { return transport_ && transport_->is_ready(); }

std::string UniTransmit::last_error() const {
    return transport_ ? transport_->last_error() : std::string("transport not started");
}

void UniTransmit::set_receive_callback(ReceiveCallback callback) {
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        receive_callback_ = std::move(callback);
    }
    if (receive_callback_) {
        start_receiver();
    } else {
        stop_receiver();
    }
}

UniTransmit::ReceiveContext UniTransmit::context() const { return {ifname_, scheme_}; }

void UniTransmit::poll_receive() {
    ReceiveCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = receive_callback_;
    }
    if (!callback) {
        return;
    }
    if (in_waiting() == 0) {
        auto first = read();
        if (first.empty()) {
            return;
        }
        auto rest = read_all();
        if (!rest.empty()) {
            first.insert(first.end(), rest.begin(), rest.end());
        }
        callback(first, context());
        return;
    }
    auto data = read_all();
    if (!data.empty()) {
        callback(data, context());
    }
}

void UniTransmit::start() {
    if (transport_) {
        return;
    }
    UrlParts parts = parse_ifname(ifname_);
    scheme_ = parts.scheme.empty() ? "unknown" : parts.scheme;
    Options opts = parse_options(parts);
    transport_ = make_transport(parts, opts);
    if (transport_ && receive_callback_) {
        start_receiver();
    }
}

void UniTransmit::close() {
    stop_receiver();
    transport_.reset();
}

void UniTransmit::start_receiver() {
    if (!transport_ || receiver_running_) {
        return;
    }
    receiver_stop_ = false;
    receiver_running_ = true;
    receiver_thread_ = std::thread([this]() {
        while (!receiver_stop_) {
            if (!transport_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            poll_receive();
            if (!receiver_stop_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    });
}

void UniTransmit::stop_receiver() {
    if (!receiver_running_) {
        return;
    }
    receiver_stop_ = true;
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    receiver_running_ = false;
}

} // namespace cc::neolux::utils::unitransmit
