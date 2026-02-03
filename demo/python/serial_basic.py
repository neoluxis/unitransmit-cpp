from __future__ import annotations

import sys
import time

from unitransmit import UniTransmit


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: serial_basic.py <device> [baud]")
        print("Example: serial_basic.py /dev/ttyUSB0 115200")
        sys.exit(1)

    device = sys.argv[1]
    baud = sys.argv[2] if len(sys.argv) > 2 else "115200"
    ifname = f"serial://{device}?baud={baud}"

    u = UniTransmit(ifname)
    u.write(b"ping\n")
    time.sleep(0.1)
    print("recv:", u.read_all())


if __name__ == "__main__":
    main()
