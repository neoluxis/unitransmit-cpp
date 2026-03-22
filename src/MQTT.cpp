#include "cc/neolux/utils/unitransmit/MQTT.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace cc::neolux::utils::unitransmit {
namespace {

std::atomic<unsigned long> g_mqtt_client_counter{0};

void append_u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void append_string(std::vector<std::uint8_t> &out, const std::string &value) {
    append_u16(out, static_cast<std::uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::vector<std::uint8_t> encode_remaining_length(std::size_t value) {
    std::vector<std::uint8_t> out;
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value % 128);
        value /= 128;
        if (value > 0) {
            byte |= 0x80;
        }
        out.push_back(byte);
    } while (value > 0);
    return out;
}

bool decode_remaining_length(const std::vector<std::uint8_t> &data, std::size_t &value,
                             std::size_t &used) {
    value = 0;
    used = 0;
    std::size_t multiplier = 1;
    for (std::uint8_t byte : data) {
        value += static_cast<std::size_t>(byte & 0x7F) * multiplier;
        ++used;
        if ((byte & 0x80) == 0) {
            return true;
        }
        multiplier *= 128;
        if (used >= 4) {
            return false;
        }
    }
    return false;
}

std::string make_client_id() {
    const auto id = g_mqtt_client_counter.fetch_add(1, std::memory_order_relaxed);
    return "unitransmit-" + std::to_string(id);
}

} // namespace

MqttTransport::MqttTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    tx_topic_ = !opts.tx_topic.empty() ? opts.tx_topic : opts.topic;
    rx_topic_ = !opts.rx_topic.empty() ? opts.rx_topic : opts.topic;
    client_id_ = !opts.client_id.empty() ? opts.client_id : make_client_id();

    if (!connect_broker(parts)) {
        return;
    }
    if (!send_connect() || !receive_connack()) {
        if (last_error_.empty()) {
            set_last_error("mqtt connect handshake failed");
        }
        close_socket_only();
        return;
    }
    if (!rx_topic_.empty() && !send_subscribe()) {
        if (last_error_.empty()) {
            set_last_error("mqtt subscribe failed");
        }
        close_socket_only();
        return;
    }

    set_last_error({});
    connected_ = true;
    receiver_thread_ = std::thread([this]() { receiver_loop(); });
}

MqttTransport::~MqttTransport() {
    stop_ = true;
    if (connected_) {
        send_packet({0xE0, 0x00});
    }
    close_socket_only();
    queue_cv_.notify_all();
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
}

std::size_t MqttTransport::in_waiting() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
}

std::vector<std::uint8_t> MqttTransport::read() { return read(1); }

std::vector<std::uint8_t> MqttTransport::read_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_.empty() && opts_.blocking && !wait_for_data(lock)) {
        return {};
    }
    return take_bytes(queue_.size());
}

std::vector<std::uint8_t> MqttTransport::read(std::size_t max_bytes) {
    if (max_bytes == 0) {
        return {};
    }
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_.empty() && opts_.blocking && !wait_for_data(lock)) {
        return {};
    }
    return take_bytes(std::min(max_bytes, queue_.size()));
}

std::size_t MqttTransport::write(const std::uint8_t *data, std::size_t size) {
    if (!connected_ || !data || size == 0 || tx_topic_.empty()) {
        if (tx_topic_.empty()) {
            set_last_error("mqtt tx topic is empty");
        } else if (!connected_ && last_error_.empty()) {
            set_last_error("mqtt broker is not connected");
        }
        return 0;
    }
    std::vector<std::uint8_t> variable_header;
    append_string(variable_header, tx_topic_);

    std::vector<std::uint8_t> packet;
    packet.push_back(0x30);
    auto remaining = encode_remaining_length(variable_header.size() + size);
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), variable_header.begin(), variable_header.end());
    packet.insert(packet.end(), data, data + size);

    return send_packet(packet) ? size : 0;
}

bool MqttTransport::connect_broker(const UrlParts &parts) {
    ensure_wsa();
    const std::string host = parts.host.empty() ? "127.0.0.1" : parts.host;
    const int port = parts.port > 0 ? parts.port : 1883;

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == kInvalidSocket) {
        set_last_error("mqtt socket creation failed");
        return false;
    }

    sockaddr_storage addr{};
    socklen_t addr_len = 0;
    if (!resolve_address(host, port, addr, addr_len, SOCK_STREAM)) {
        set_last_error("mqtt broker address resolution failed");
        close_socket_only();
        return false;
    }
    if (connect(sock_, reinterpret_cast<sockaddr *>(&addr), addr_len) != 0) {
        set_last_error("mqtt broker connect failed");
        close_socket_only();
        return false;
    }
    return true;
}

bool MqttTransport::send_packet(const std::vector<std::uint8_t> &packet) {
    if (sock_ == kInvalidSocket || packet.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(send_mutex_);
    std::size_t offset = 0;
    while (offset < packet.size()) {
        int rc = send(sock_, reinterpret_cast<const char *>(packet.data() + offset),
                      static_cast<int>(packet.size() - offset), 0);
        if (rc <= 0) {
            connected_ = false;
            set_last_error("mqtt send failed");
            return false;
        }
        offset += static_cast<std::size_t>(rc);
    }
    return true;
}

bool MqttTransport::send_connect() {
    std::vector<std::uint8_t> payload;
    append_string(payload, "MQTT");
    payload.push_back(0x04);
    payload.push_back(0x02);
    append_u16(payload, static_cast<std::uint16_t>(std::max(0, opts_.keep_alive_sec)));
    append_string(payload, client_id_);

    std::vector<std::uint8_t> packet;
    packet.push_back(0x10);
    auto remaining = encode_remaining_length(payload.size());
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), payload.begin(), payload.end());
    return send_packet(packet);
}

bool MqttTransport::receive_connack() {
    std::uint8_t header = 0;
    std::vector<std::uint8_t> payload;
    if (!recv_packet(header, payload, opts_.timeout_ms)) {
        set_last_error("mqtt connack timeout or read failure");
        return false;
    }
    if (!(header == 0x20 && payload.size() == 2 && payload[1] == 0x00)) {
        set_last_error("mqtt connack rejected");
        return false;
    }
    return true;
}

bool MqttTransport::send_subscribe() {
    std::vector<std::uint8_t> variable_header;
    append_u16(variable_header, 1);
    append_string(variable_header, rx_topic_);
    variable_header.push_back(0x00);

    std::vector<std::uint8_t> packet;
    packet.push_back(0x82);
    auto remaining = encode_remaining_length(variable_header.size());
    packet.insert(packet.end(), remaining.begin(), remaining.end());
    packet.insert(packet.end(), variable_header.begin(), variable_header.end());
    return send_packet(packet);
}

void MqttTransport::receiver_loop() {
    while (!stop_ && connected_) {
        std::uint8_t header = 0;
        std::vector<std::uint8_t> payload;
        if (!recv_packet(header, payload, -1)) {
            if (!stop_) {
                set_last_error("mqtt receive loop stopped");
            }
            break;
        }
        const std::uint8_t packet_type = header >> 4;
        if (packet_type == 3) {
            if (payload.size() < 2) {
                continue;
            }
            const std::size_t topic_len = (static_cast<std::size_t>(payload[0]) << 8) | payload[1];
            if (payload.size() < 2 + topic_len) {
                continue;
            }
            const std::size_t payload_offset = 2 + topic_len;
            std::vector<std::uint8_t> message(payload.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                                              payload.end());
            enqueue_payload(message);
        } else if (packet_type == 13) {
            continue;
        } else if (packet_type == 9) {
            continue;
        } else {
            continue;
        }
    }
    connected_ = false;
    queue_cv_.notify_all();
}

bool MqttTransport::recv_exact(std::uint8_t *data, std::size_t size, int timeout_ms) {
    if (sock_ == kInvalidSocket || !data) {
        set_last_error("mqtt socket is not available");
        return false;
    }
    std::size_t offset = 0;
    while (offset < size) {
        if (!wait_for_read(sock_, timeout_ms)) {
            set_last_error("mqtt read timeout");
            return false;
        }
        int rc = recv(sock_, reinterpret_cast<char *>(data + offset), static_cast<int>(size - offset), 0);
        if (rc <= 0) {
            set_last_error("mqtt recv failed");
            return false;
        }
        offset += static_cast<std::size_t>(rc);
    }
    return true;
}

bool MqttTransport::recv_packet(std::uint8_t &header, std::vector<std::uint8_t> &payload, int timeout_ms) {
    payload.clear();
    if (!recv_exact(&header, 1, timeout_ms)) {
        return false;
    }

    std::vector<std::uint8_t> encoded_length;
    for (int i = 0; i < 4; ++i) {
        std::uint8_t byte = 0;
        if (!recv_exact(&byte, 1, timeout_ms)) {
            return false;
        }
        encoded_length.push_back(byte);
        if ((byte & 0x80) == 0) {
            break;
        }
    }

    std::size_t remaining = 0;
    std::size_t used = 0;
    if (!decode_remaining_length(encoded_length, remaining, used)) {
        set_last_error("mqtt remaining length decode failed");
        return false;
    }
    payload.resize(remaining);
    if (remaining == 0) {
        return true;
    }
    return recv_exact(payload.data(), remaining, timeout_ms);
}

void MqttTransport::enqueue_payload(const std::vector<std::uint8_t> &payload) {
    if (payload.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.insert(queue_.end(), payload.begin(), payload.end());
    }
    queue_cv_.notify_all();
}

bool MqttTransport::wait_for_data(std::unique_lock<std::mutex> &lock) const {
    if (!queue_.empty()) {
        return true;
    }
    if (!connected_) {
        return false;
    }
    if (opts_.timeout_ms < 0) {
        queue_cv_.wait(lock, [this]() { return !queue_.empty() || !connected_; });
        return !queue_.empty();
    }
    return queue_cv_.wait_for(lock, std::chrono::milliseconds(opts_.timeout_ms),
                              [this]() { return !queue_.empty() || !connected_; }) &&
           !queue_.empty();
}

std::vector<std::uint8_t> MqttTransport::take_bytes(std::size_t max_bytes) {
    std::vector<std::uint8_t> out;
    out.reserve(max_bytes);
    while (!queue_.empty() && out.size() < max_bytes) {
        out.push_back(queue_.front());
        queue_.pop_front();
    }
    return out;
}

void MqttTransport::close_socket_only() {
    if (sock_ != kInvalidSocket) {
        close_socket(sock_);
        sock_ = kInvalidSocket;
    }
}

bool MqttTransport::is_ready() const { return connected_; }

std::string MqttTransport::last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

void MqttTransport::set_last_error(std::string value) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = std::move(value);
}

} // namespace cc::neolux::utils::unitransmit
