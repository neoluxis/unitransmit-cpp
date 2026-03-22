#include "cc/neolux/utils/unitransmit/Unitransmit.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    {
        UniTransmit central(
            "gatt://central/link?remote=peripheral&tx=cmd&rx=evt&blocking=1&timeout_ms=200");
        UniTransmit peripheral(
            "gatt://peripheral/link?remote=central&tx=evt&rx=cmd&blocking=1&timeout_ms=200");

        const std::string request = "ping";
        assert(central.write(request) == request.size());
        auto request_bytes = peripheral.read_all();
        std::string request_out(reinterpret_cast<const char *>(request_bytes.data()),
                                request_bytes.size());
        assert(request_out == request);

        const std::string response = "pong";
        assert(peripheral.write(response) == response.size());
        auto response_bytes = central.read_all();
        std::string response_out(reinterpret_cast<const char *>(response_bytes.data()),
                                 response_bytes.size());
        assert(response_out == response);
    }

    {
        UniTransmit central(
            "gatt://central/chunk?remote=peripheral&service=test&tx=data&rx=notify&blocking=1&timeout_ms=200");
        UniTransmit peripheral(
            "gatt://peripheral/ignored?remote=central&service=test&tx=notify&rx=data&blocking=1&timeout_ms=200");

        const std::string payload = "hello";
        central.write(payload);
        auto head = peripheral.read(2);
        assert(head.size() == 2);
        assert(peripheral.in_waiting() == 3);
        auto tail = peripheral.read_all();
        std::string merged(reinterpret_cast<const char *>(head.data()), head.size());
        merged.append(reinterpret_cast<const char *>(tail.data()), tail.size());
        assert(merged == payload);
    }

    {
        UniTransmit orphan(
            "gatt://solo/link?remote=missing&tx=cmd&rx=evt&blocking=0&timeout_ms=10");
        assert(orphan.write("noop") == 0);
        assert(orphan.read_all().empty());
    }

    {
        UniTransmit reader(
            "gatt://reader/svc?remote=writer&tx=cmd&rx=evt&blocking=1&timeout_ms=500");
        UniTransmit writer(
            "gatt://writer/svc?remote=reader&tx=evt&rx=cmd&blocking=1&timeout_ms=500");

        std::thread producer([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            writer.write("ok");
        });
        auto data = reader.read_all();
        producer.join();
        std::string out(reinterpret_cast<const char *>(data.data()), data.size());
        assert(out == "ok");
    }

    return 0;
}
