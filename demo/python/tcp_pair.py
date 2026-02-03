from __future__ import annotations

import time

from unitransmit import UniTransmit


def main() -> None:
    server = UniTransmit("tcp-s://0.0.0.0:9100")
    client = UniTransmit("tcp://127.0.0.1:9100")

    client.write(b"hello")
    time.sleep(0.1)

    print("recv:", server.read_all())


if __name__ == "__main__":
    main()
