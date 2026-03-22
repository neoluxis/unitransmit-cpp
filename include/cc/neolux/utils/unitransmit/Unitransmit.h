#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace cc::neolux::utils::unitransmit {

class ReceiveDispatcher;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual std::size_t in_waiting() const = 0;
    virtual std::vector<std::uint8_t> read() = 0;
    virtual std::vector<std::uint8_t> read_all() = 0;
    virtual std::vector<std::uint8_t> read(std::size_t max_bytes) = 0;
    virtual std::size_t write(const std::uint8_t *data, std::size_t size) = 0;
    virtual bool is_ready() const;
    virtual std::string last_error() const;

    std::size_t write(const std::vector<std::uint8_t> &data);
    std::size_t write(const std::string &data);
};

class UniTransmit {
public:
    struct ReceiveContext {
        std::string ifname;
        std::string scheme;
    };

    using ReceiveCallback = std::function<void(const std::vector<std::uint8_t> &, const ReceiveContext &)>;

    explicit UniTransmit(std::string ifname);
    ~UniTransmit();

    std::vector<std::uint8_t> read();
    std::vector<std::uint8_t> read_all();
    std::vector<std::uint8_t> read(std::size_t max_bytes);
    std::size_t write(const std::vector<std::uint8_t> &data);
    std::size_t write(const std::string &data);
    std::size_t in_waiting() const;

    UniTransmit &operator<<(const std::string &data);
    UniTransmit &operator<<(const std::vector<std::uint8_t> &data);
    UniTransmit &operator>>(std::string &out);
    UniTransmit &operator>>(std::vector<std::uint8_t> &out);

    const std::string &ifname() const;
    const std::string &scheme() const;
    bool is_ready() const;
    std::string last_error() const;

    void set_receive_callback(ReceiveCallback callback);
    void start();
    void close();

private:
    void poll_receive();
    ReceiveContext context() const;
    void start_receiver();
    void stop_receiver();

    std::string ifname_;
    std::string scheme_;
    std::unique_ptr<ITransport> transport_;
    mutable std::mutex callback_mutex_;
    ReceiveCallback receive_callback_;
    std::thread receiver_thread_;
    bool receiver_running_ = false;
    bool receiver_stop_ = false;
};

} // namespace cc::neolux::utils::unitransmit
