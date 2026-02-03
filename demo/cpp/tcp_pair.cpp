#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    UniTransmit server("tcp-s://0.0.0.0:9100");
    UniTransmit client("tcp://127.0.0.1:9100");

    client.write(std::string("hello"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto data = server.read_all();
    std::string out(reinterpret_cast<const char *>(data.data()), data.size());
    std::cout << "recv: " << out << "\n";

    return 0;
}
