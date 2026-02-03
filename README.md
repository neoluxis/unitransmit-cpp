# UniTransmit

UniTransmit is a small C++ library that provides a unified interface for multiple transport types
(TCP, UDP, Serial). You create a `UniTransmit` instance with an `ifname` URI, then read/write with
methods or stream operators.

For full usage details (URI formats, API summary, terminal simulator), see:
- `doc/usage.md`

Quick build:

```bash
cmake -S . -B build
cmake --build build
```
