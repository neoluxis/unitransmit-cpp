#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    const char *run_net = std::getenv("UNITRANSMIT_RUN_NET_TESTS");
    if (!run_net || std::string(run_net) != "1") {
        return 0;
    }

    const int port_a = 39002;
    const int port_b = 39003;
    UniTransmit a("udp://127.0.0.1:" + std::to_string(port_a) +
                  "?remote=127.0.0.1:" + std::to_string(port_b) +
                  "&blocking=1&timeout_ms=2000");
    UniTransmit b("udp://127.0.0.1:" + std::to_string(port_b) +
                  "?remote=127.0.0.1:" + std::to_string(port_a) +
                  "&blocking=1&timeout_ms=2000");

    const std::string msg1 = "hello";
    a.write(msg1);
    auto b_read = b.read_all();
    std::string b_out(reinterpret_cast<const char *>(b_read.data()), b_read.size());
    assert(b_out == msg1);

    const std::string msg2 = "world";
    b.write(msg2);
    auto a_read = a.read_all();
    std::string a_out(reinterpret_cast<const char *>(a_read.data()), a_read.size());
    assert(a_out == msg2);

    return 0;
}
