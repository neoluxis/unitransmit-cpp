# UniTransmit demos

Quick hands-on examples for C++ and Python.

## C++ (loopback)
Build (from repo root):

```
cmake -S . -B build
cmake --build build
```

Run:

```
./build/unitransmit_demo_loopback
```

## C++ (UDP pair)

```
./build/unitransmit_demo_udp_pair
```

## C++ (TCP pair)

```
./build/unitransmit_demo_tcp_pair
```

## Python (loopback)

```
./.conda/bin/python -m pip install .
./.conda/bin/python demo/python/loopback.py
```

## Python (UDP pair)
Install the package in your env:

```
./.conda/bin/python -m pip install .
```

Run:

```
./.conda/bin/python demo/python/udp_pair.py
```

## Python (TCP pair)

```
./.conda/bin/python demo/python/tcp_pair.py
```

## Python (serial)

```
./.conda/bin/python demo/python/serial_basic.py /dev/ttyUSB0 115200
```
