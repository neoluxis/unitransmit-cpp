#pragma once

#include "cc/neolux/utils/unitransmit/Protocol.h"
#include "cc/neolux/utils/unitransmit/Unitransmit.h"
#include "cc/neolux/utils/unitransmit/Utils.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cc::neolux::utils::unitransmit {

struct MqttConfig {
    std::string host = "127.0.0.1";
    int port = 1883;
    std::string client_id;
    std::string tx_topic;
    std::string rx_topic;
    int keep_alive_sec = 30;
    bool blocking = true;
    int timeout_ms = -1;

    static MqttConfig from_url(const UrlParts &parts, const Options &opts);
};

class MqttTransport final : public ITransport {
public:
    explicit MqttTransport(const MqttConfig &config);
    MqttTransport(const UrlParts &parts, const Options &opts);
    ~MqttTransport() override;

    std::size_t in_waiting() const override;
    std::vector<std::uint8_t> read() override;
    std::vector<std::uint8_t> read_all() override;
    std::vector<std::uint8_t> read(std::size_t max_bytes) override;
    std::size_t write(const std::uint8_t *data, std::size_t size) override;
    bool is_ready() const override;
    std::string last_error() const override;

private:
    bool connect_broker();
    bool send_packet(const std::vector<std::uint8_t> &packet);
    bool send_connect();
    bool receive_connack();
    bool send_subscribe();
    void receiver_loop();
    bool recv_exact(std::uint8_t *data, std::size_t size, int timeout_ms);
    bool recv_packet(std::uint8_t &header, std::vector<std::uint8_t> &payload, int timeout_ms);
    void enqueue_payload(const std::vector<std::uint8_t> &payload);
    bool wait_for_data(std::unique_lock<std::mutex> &lock) const;
    std::vector<std::uint8_t> take_bytes(std::size_t max_bytes);
    void close_socket_only();
    void set_last_error(std::string value);

    MqttConfig config_;
    SocketHandle sock_ = kInvalidSocket;
    mutable std::mutex queue_mutex_;
    mutable std::condition_variable queue_cv_;
    std::deque<std::uint8_t> queue_;
    std::mutex send_mutex_;
    mutable std::mutex error_mutex_;
    std::string last_error_;
    std::thread receiver_thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
};

} // namespace cc::neolux::utils::unitransmit
