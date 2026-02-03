# UniTransmit

UniTransmit is a small C++ library that provides a unified interface for multiple transport types
(TCP, UDP, Serial). You create a `UniTransmit` instance with an `ifname` URI, then read/write with
methods or stream operators.

## Build

```bash
cmake -S . -B build
cmake --build build
```

The build produces:
- `libunitransmit.a` (static)
- `libunitransmit.so` (shared)

## Quick Start

```cpp
#include "cc/neolux/utils/unitransmit/Unitransmit.h"

using cc::neolux::utils::unitransmit::UniTransmit;

int main() {
    UniTransmit tcp("tcp://127.0.0.1:9000?blocking=1&timeout_ms=1000");
    tcp.write("hello");

    auto one = tcp.read();       // reads 1 byte
    auto some = tcp.read(4);     // reads up to 4 bytes
    auto all = tcp.read_all();   // reads all available data

    std::string text;
    tcp >> text;                 // reads all available data into string

    return 0;
}
```

## URI Format

General form:

```
<scheme>://<host>:<port>?<query>
```

Supported schemes:
- TCP client: `tcp://host:port`
- TCP server: `tcp-s://0.0.0.0:port` (server mode)
- UDP server: `udp://host:port`
- UDP client: `udp://0.0.0.0:0?remote=host:port`
- Serial: `serial:///dev/ttyUSB0?baud=115200` (Linux)
- Serial: `serial://COM3?baud=115200` (Windows)

Common query parameters:
- `blocking=1|0` (default 1)
- `timeout_ms=<int>` (default -1, no timeout)
- `max_connect=<int>` (TCP/UDP server: max tracked clients/peers; 1 by default, 0 = unlimited)
- `remote=host:port` (UDP client destination)
- `broadcast=1|0` (UDP: send to broadcast address `255.255.255.255:port`)
- `baud=<int>` (serial)
- `data=<int>` (serial data bits)
- `stop=<int>` (serial stop bits)
- `parity=n|e|o` (serial parity)

## API Summary

```cpp
std::vector<uint8_t> read();        // reads 1 byte
std::vector<uint8_t> read(size_t);  // reads up to N bytes
std::vector<uint8_t> read_all();    // reads all available data
size_t write(const std::string &);
size_t write(const std::vector<uint8_t> &);
size_t in_waiting() const;
```

Stream operators:

```cpp
u << "hello";      // write string
u >> out_string;   // read all available data
```

## Receive Callback (Background Thread)

You can register a callback that runs whenever data is available. Each instance runs
its own background thread to invoke the callback with the received bytes plus a small context.

```cpp
UniTransmit u("udp://0.0.0.0:8888?max_connect=0");
u.set_receive_callback([](const std::vector<uint8_t> &data,
                           const UniTransmit::ReceiveContext &ctx) {
    // ctx.ifname, ctx.scheme
    (void)ctx;
    // handle data
});
```

To stop receiving, call:

```cpp
u.set_receive_callback(nullptr);
```

## Terminal Simulator

A simple CLI tool is provided for interactive testing.

Build:

```bash
cmake -S . -B build
cmake --build build --target unitransmit_terminal_sim
```

Run:

```bash
./build/unitransmit_terminal_sim
```

Commands:
- `help`
- `list`
- `create <name> <ifname>`
- `delete <name>`
- `write <name> <text>`
- `read <name>`
- `readn <name> <n>`
- `readall <name>`
- `in_waiting <name>`
- `quit`

Example session:

```
> create s tcp-s://127.0.0.1:9000?blocking=1&timeout_ms=2000
> create c tcp://127.0.0.1:9000?blocking=1&timeout_ms=2000
> write c hello
> readall s
```
