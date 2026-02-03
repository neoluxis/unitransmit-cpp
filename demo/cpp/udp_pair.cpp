#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    UniTransmit a("udp://0.0.0.0:9000?remote=127.0.0.1:9001");
    UniTransmit b("udp://0.0.0.0:9001?remote=127.0.0.1:9000");

    a.write(std::string("hello"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto data = b.read_all();
    std::string out(reinterpret_cast<const char *>(data.data()), data.size());
    std::cout << "recv: " << out << "\n";

    return 0;
}
