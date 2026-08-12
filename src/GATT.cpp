#include "cc/neolux/utils/unitransmit/GATT.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <tuple>
#include <utility>

namespace cc::neolux::utils::unitransmit {
namespace {

using EndpointKey = std::tuple<std::string, std::string, std::string>;

std::mutex g_registry_mutex;
std::map<EndpointKey, std::weak_ptr<GattTransport::EndpointBuffer>> g_registry;

std::string query_value(const UrlParts &parts, const char *key) {
    auto it = parts.query.find(key);
    if (it == parts.query.end()) {
        return {};
    }
    return it->second;
}

std::string non_empty_or(std::string value, const std::string &fallback) {
    return value.empty() ? fallback : value;
}

EndpointKey make_key(const std::string &device, const std::string &service,
                     const std::string &characteristic) {
    return {device, service, characteristic};
}

std::shared_ptr<GattTransport::EndpointBuffer> register_endpoint(const EndpointKey &key) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto &slot = g_registry[key];
    auto existing = slot.lock();
    if (existing) {
        return existing;
    }
    auto created = std::make_shared<GattTransport::EndpointBuffer>();
    slot = created;
    return created;
}

std::shared_ptr<GattTransport::EndpointBuffer> find_endpoint(const EndpointKey &key) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto it = g_registry.find(key);
    if (it == g_registry.end()) {
        return nullptr;
    }
    auto endpoint = it->second.lock();
    if (!endpoint) {
        g_registry.erase(it);
    }
    return endpoint;
}

void unregister_endpoint(const EndpointKey &key,
                         const std::shared_ptr<GattTransport::EndpointBuffer> &endpoint) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto it = g_registry.find(key);
    if (it == g_registry.end()) {
        return;
    }
    auto current = it->second.lock();
    if (!current || current == endpoint) {
        g_registry.erase(it);
    }
}

} // namespace

// --- GattConfig -------------------------------------------------------------

GattConfig GattConfig::from_url(const UrlParts &parts, const Options &opts) {
    GattConfig cfg;
    cfg.local_device = non_empty_or(query_value(parts, "device"), parts.host);
    cfg.remote_device = query_value(parts, "remote");
    cfg.service = non_empty_or(query_value(parts, "service"), parts.path);
    cfg.tx_char = query_value(parts, "tx");
    cfg.rx_char = query_value(parts, "rx");

    if (cfg.service.empty()) {
        cfg.service = "default-service";
    }
    if (cfg.tx_char.empty()) {
        cfg.tx_char = "tx";
    }
    if (cfg.rx_char.empty()) {
        cfg.rx_char = "rx";
    }
    if (cfg.local_device.empty()) {
        cfg.local_device = "gatt-local";
    }

    cfg.blocking = opts.blocking;
    cfg.timeout_ms = opts.timeout_ms;
    return cfg;
}

// --- GattTransport ----------------------------------------------------------

GattTransport::GattTransport(const GattConfig &config) : config_(config) {
    inbound_ = register_endpoint(make_key(config_.local_device, config_.service, config_.rx_char));
}

GattTransport::GattTransport(const UrlParts &parts, const Options &opts)
    : GattTransport(GattConfig::from_url(parts, opts)) {}

GattTransport::~GattTransport() {
    unregister_endpoint(make_key(config_.local_device, config_.service, config_.rx_char), inbound_);
}

std::size_t GattTransport::in_waiting() const {
    if (!inbound_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(inbound_->mutex);
    return inbound_->bytes.size();
}

std::vector<std::uint8_t> GattTransport::read() { return read(1); }

std::vector<std::uint8_t> GattTransport::read_all() {
    if (!inbound_) {
        return {};
    }
    std::unique_lock<std::mutex> lock(inbound_->mutex);
    if (inbound_->bytes.empty() && config_.blocking && !wait_for_data(lock)) {
        return {};
    }
    std::vector<std::uint8_t> out;
    out.reserve(inbound_->bytes.size());
    while (!inbound_->bytes.empty()) {
        out.push_back(inbound_->bytes.front());
        inbound_->bytes.pop_front();
    }
    return out;
}

std::vector<std::uint8_t> GattTransport::read(std::size_t max_bytes) {
    if (!inbound_ || max_bytes == 0) {
        return {};
    }
    std::unique_lock<std::mutex> lock(inbound_->mutex);
    if (inbound_->bytes.empty() && config_.blocking && !wait_for_data(lock)) {
        return {};
    }
    if (inbound_->bytes.empty()) {
        return {};
    }
    const std::size_t count = std::min(max_bytes, inbound_->bytes.size());
    std::vector<std::uint8_t> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(inbound_->bytes.front());
        inbound_->bytes.pop_front();
    }
    return out;
}

std::size_t GattTransport::write(const std::uint8_t *data, std::size_t size) {
    if (!data || size == 0 || config_.remote_device.empty()) {
        return 0;
    }
    auto outbound = find_endpoint(make_key(config_.remote_device, config_.service, config_.tx_char));
    if (!outbound) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(outbound->mutex);
        outbound->bytes.insert(outbound->bytes.end(), data, data + size);
    }
    outbound->cv.notify_all();
    return size;
}

bool GattTransport::wait_for_data(std::unique_lock<std::mutex> &lock) const {
    if (!inbound_) {
        return false;
    }
    if (!inbound_->bytes.empty()) {
        return true;
    }
    if (config_.timeout_ms < 0) {
        inbound_->cv.wait(lock, [this]() { return !inbound_->bytes.empty(); });
        return !inbound_->bytes.empty();
    }
    return inbound_->cv.wait_for(lock, std::chrono::milliseconds(config_.timeout_ms),
                                 [this]() { return !inbound_->bytes.empty(); });
}

} // namespace cc::neolux::utils::unitransmit
