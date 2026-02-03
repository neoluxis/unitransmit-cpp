#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    const char *serial_a = std::getenv("UNITRANSMIT_SERIAL_A");
    const char *serial_b = std::getenv("UNITRANSMIT_SERIAL_B");
    if (!serial_a || !serial_b) {
        return 0;
    }

    UniTransmit a(std::string("serial://") + serial_a + "?baud=115200&blocking=1&timeout_ms=2000");
    UniTransmit b(std::string("serial://") + serial_b + "?baud=115200&blocking=1&timeout_ms=2000");

    const std::string msg1 = "ab";
    a.write(msg1);
    auto b_read = b.read_all();
    std::string b_out(reinterpret_cast<const char *>(b_read.data()), b_read.size());
    assert(b_out == msg1);

    const std::string msg2 = "cd";
    b.write(msg2);
    auto a_read = a.read_all();
    std::string a_out(reinterpret_cast<const char *>(a_read.data()), a_read.size());
    assert(a_out == msg2);

    return 0;
}
