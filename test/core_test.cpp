#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cassert>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    {
        UniTransmit u("loop");
        assert(u.scheme() == "loop");
        assert(u.in_waiting() == 0);

        std::string payload = "hello";
        u << payload;
        assert(u.in_waiting() == payload.size());

        std::string out;
        u >> out;
        assert(out == payload);
        assert(u.in_waiting() == 0);
    }

    {
        UniTransmit u("loop://buffer");
        assert(u.scheme() == "loop");

        std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
        u.write(data);
        assert(u.in_waiting() == data.size());

        auto out = u.read(3);
        assert(out.size() == 3);
        assert(u.in_waiting() == 2);

        auto rest = u.read_all();
        assert(rest.size() == 2);
        assert(u.in_waiting() == 0);
    }

    {
        UniTransmit u("loop");
        assert(u.scheme() == "loop");

        const std::string payload = "abc";
        u.write(payload);
        std::string out;
        u >> out;
        assert(out == payload);
    }

    {
        UniTransmit u("loop");
        std::mutex received_mutex;
        std::string received;
        std::string received_ifname;
        std::string received_scheme;
        u.set_receive_callback([&](const std::vector<std::uint8_t> &data,
                                   const UniTransmit::ReceiveContext &ctx) {
            std::lock_guard<std::mutex> lock(received_mutex);
            received.assign(reinterpret_cast<const char *>(data.data()), data.size());
            received_ifname = ctx.ifname;
            received_scheme = ctx.scheme;
        });

        u.write(std::string("cb"));
        for (int i = 0; i < 50 && received.empty(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        {
            std::lock_guard<std::mutex> lock(received_mutex);
            assert(received == "cb");
            assert(received_ifname == "loop");
            assert(received_scheme == "loop");
        }
        u.set_receive_callback(nullptr);
    }

    return 0;
}
