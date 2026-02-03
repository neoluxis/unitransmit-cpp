from __future__ import annotations

from unitransmit import UniTransmit


def main() -> None:
    u = UniTransmit("loop")
    u.write(b"hello")
    print("recv:", u.read_all())


if __name__ == "__main__":
    main()
