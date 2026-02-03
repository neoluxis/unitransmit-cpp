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

    const int base_port = 39001;
    UniTransmit server("tcp-s://127.0.0.1:" + std::to_string(base_port) +
                       "?blocking=1&timeout_ms=2000&max_connect=1");
    UniTransmit client("tcp://127.0.0.1:" + std::to_string(base_port) +
                       "?blocking=1&timeout_ms=2000");

    const std::string ping = "ping";
    client.write(ping);
    auto server_read = server.read_all();
    std::string server_out(reinterpret_cast<const char *>(server_read.data()), server_read.size());
    assert(server_out == ping);

    const std::string pong = "pong";
    server.write(pong);
    auto client_read = client.read_all();
    std::string client_out(reinterpret_cast<const char *>(client_read.data()), client_read.size());
    assert(client_out == pong);

    return 0;
}
