from __future__ import annotations

import time

from unitransmit import UniTransmit


def main() -> None:
    u1 = UniTransmit("udp://0.0.0.0:9000?remote=127.0.0.1:9001")
    u2 = UniTransmit("udp://0.0.0.0:9001?remote=127.0.0.1:9000")

    def on_recv(data: bytes, ctx: dict) -> None:
        print(f"recv: {data!r} from {ctx}")

    u2.set_receive_callback(on_recv)

    print("send ->", u1.write(b"hello"))
    time.sleep(0.1)

    u1.close()
    u2.close()


if __name__ == "__main__":
    main()
