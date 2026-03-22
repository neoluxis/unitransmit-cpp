#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using cc::neolux::utils::unitransmit::UniTransmit;

namespace {

#ifdef _WIN32
using TestSocket = SOCKET;
const TestSocket kInvalidTestSocket = INVALID_SOCKET;
void close_test_socket(TestSocket sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}
#else
using TestSocket = int;
const TestSocket kInvalidTestSocket = -1;
void close_test_socket(TestSocket sock) {
    if (sock >= 0) {
        close(sock);
    }
}
#endif

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

bool recv_exact(TestSocket sock, std::uint8_t *data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        int rc = recv(sock, reinterpret_cast<char *>(data + offset), static_cast<int>(size - offset), 0);
        if (rc <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(rc);
    }
    return true;
}

bool recv_packet(TestSocket sock, std::uint8_t &header, std::vector<std::uint8_t> &payload) {
    payload.clear();
    if (!recv_exact(sock, &header, 1)) {
        return false;
    }
    std::size_t remaining = 0;
    std::size_t multiplier = 1;
    for (int i = 0; i < 4; ++i) {
        std::uint8_t byte = 0;
        if (!recv_exact(sock, &byte, 1)) {
            return false;
        }
        remaining += static_cast<std::size_t>(byte & 0x7F) * multiplier;
        if ((byte & 0x80) == 0) {
            break;
        }
        multiplier *= 128;
    }
    payload.resize(remaining);
    return remaining == 0 || recv_exact(sock, payload.data(), remaining);
}

bool send_packet(TestSocket sock, const std::vector<std::uint8_t> &packet) {
    std::size_t offset = 0;
    while (offset < packet.size()) {
        int rc = send(sock, reinterpret_cast<const char *>(packet.data() + offset),
                      static_cast<int>(packet.size() - offset), 0);
        if (rc <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(rc);
    }
    return true;
}

class TestBroker {
public:
    explicit TestBroker(int port) : port_(port) {
        server_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_ == kInvalidTestSocket) {
            return;
        }

        int reuse = 1;
#ifdef _WIN32
        setsockopt(server_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
        setsockopt(server_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(port_));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(server_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            close_test_socket(server_);
            server_ = kInvalidTestSocket;
            return;
        }
        if (listen(server_, 8) != 0) {
            close_test_socket(server_);
            server_ = kInvalidTestSocket;
            return;
        }

        thread_ = std::thread([this]() { run(); });
    }

    ~TestBroker() {
        stop_ = true;
        close_test_socket(server_);
        if (thread_.joinable()) {
            thread_.join();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &[sock, _] : clients_) {
            close_test_socket(sock);
        }
    }

    bool valid() const { return server_ != kInvalidTestSocket; }

private:
    void run() {
        while (!stop_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            TestSocket client = accept(server_, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
            if (client == kInvalidTestSocket) {
                if (stop_) {
                    break;
                }
                continue;
            }
            std::thread(&TestBroker::handle_client, this, client).detach();
        }
    }

    void handle_client(TestSocket client) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clients_[client] = "";
        }

        for (;;) {
            std::uint8_t header = 0;
            std::vector<std::uint8_t> payload;
            if (!recv_packet(client, header, payload)) {
                remove_client(client);
                return;
            }

            const std::uint8_t packet_type = header >> 4;
            if (packet_type == 1) {
                send_packet(client, {0x20, 0x02, 0x00, 0x00});
            } else if (packet_type == 8) {
                assert(payload.size() >= 5);
                const std::uint16_t packet_id = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
                std::size_t pos = 2;
                const std::uint16_t topic_len =
                    static_cast<std::uint16_t>((payload[pos] << 8) | payload[pos + 1]);
                pos += 2;
                const std::string topic(reinterpret_cast<const char *>(payload.data() + pos), topic_len);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    clients_[client] = topic;
                }
                std::vector<std::uint8_t> suback = {0x90, 0x03};
                append_u16(suback, packet_id);
                suback.push_back(0x00);
                send_packet(client, suback);
            } else if (packet_type == 3) {
                assert(payload.size() >= 2);
                const std::uint16_t topic_len = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
                const std::string topic(reinterpret_cast<const char *>(payload.data() + 2), topic_len);
                std::vector<std::uint8_t> message(payload.begin() + 2 + topic_len, payload.end());
                publish(topic, message);
            } else if (packet_type == 14) {
                remove_client(client);
                return;
            }
        }
    }

    void publish(const std::string &topic, const std::vector<std::uint8_t> &message) {
        std::vector<TestSocket> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto &[sock, sub_topic] : clients_) {
                if (sub_topic == topic) {
                    targets.push_back(sock);
                }
            }
        }

        std::vector<std::uint8_t> packet;
        packet.push_back(0x30);
        std::vector<std::uint8_t> body;
        append_string(body, topic);
        body.insert(body.end(), message.begin(), message.end());
        auto remaining = encode_remaining_length(body.size());
        packet.insert(packet.end(), remaining.begin(), remaining.end());
        packet.insert(packet.end(), body.begin(), body.end());

        for (TestSocket sock : targets) {
            send_packet(sock, packet);
        }
    }

    void remove_client(TestSocket client) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clients_.erase(client);
        }
        close_test_socket(client);
    }

    int port_;
    TestSocket server_ = kInvalidTestSocket;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::map<TestSocket, std::string> clients_;
};

} // namespace

int main() {
    const int port = 39004;
    TestBroker broker(port);
    if (!broker.valid()) {
        return 0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        UniTransmit pub("mqtt://127.0.0.1:" + std::to_string(port) +
                        "?client_id=pub&tx=demo/up&rx=demo/down&blocking=1&timeout_ms=1000");
        UniTransmit sub("mqtt://127.0.0.1:" + std::to_string(port) +
                        "?client_id=sub&tx=demo/down&rx=demo/up&blocking=1&timeout_ms=1000");

        const std::string ping = "ping";
        assert(pub.write(ping) == ping.size());
        auto got = sub.read_all();
        std::string got_text(reinterpret_cast<const char *>(got.data()), got.size());
        assert(got_text == ping);

        const std::string pong = "pong";
        assert(sub.write(pong) == pong.size());
        auto reply = pub.read_all();
        std::string reply_text(reinterpret_cast<const char *>(reply.data()), reply.size());
        assert(reply_text == pong);
    }

    {
        UniTransmit left("mqtt://127.0.0.1:" + std::to_string(port) +
                         "?client_id=left&tx=chunk/out&rx=chunk/in&blocking=1&timeout_ms=1000");
        UniTransmit right("mqtt://127.0.0.1:" + std::to_string(port) +
                          "?client_id=right&tx=chunk/in&rx=chunk/out&blocking=1&timeout_ms=1000");

        left.write("hello");
        auto head = right.read(2);
        assert(head.size() == 2);
        assert(right.in_waiting() == 3);
        auto tail = right.read_all();
        std::string merged(reinterpret_cast<const char *>(head.data()), head.size());
        merged.append(reinterpret_cast<const char *>(tail.data()), tail.size());
        assert(merged == "hello");
    }

    {
        UniTransmit idle("mqtt://127.0.0.1:" + std::to_string(port) +
                         "?client_id=idle&rx=idle/topic&blocking=0&timeout_ms=50");
        assert(idle.read_all().empty());
        assert(idle.in_waiting() == 0);
    }

    return 0;
}
