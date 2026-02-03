#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    UniTransmit tx("loop");

    const std::string payload = "hello";
    tx.write(payload);

    std::string out;
    tx >> out;

    std::cout << "sent: " << payload << "\n";
    std::cout << "recv: " << out << "\n";

    std::vector<std::uint8_t> bytes = {1, 2, 3, 4};
    tx.write(bytes);

    auto read = tx.read_all();
    std::cout << "bytes size: " << read.size() << "\n";

    return 0;
}
