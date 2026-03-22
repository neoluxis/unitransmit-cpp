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

class MqttTransport final : public ITransport {
public:
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
    bool connect_broker(const UrlParts &parts);
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

    Options opts_;
    SocketHandle sock_ = kInvalidSocket;
    std::string tx_topic_;
    std::string rx_topic_;
    std::string client_id_;
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
