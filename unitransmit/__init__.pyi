from __future__ import annotations

from typing import Callable, Dict, overload

class UniTransmit:
    """Unified transport interface for multiple protocols.

    Args:
        ifname: Interface string (e.g. "udp://host:port", "tcp://host:port").
    """

    def __init__(self, ifname: str) -> None: ...

    @overload
    def read(self) -> bytes:
        """Read a single packet or chunk, if available."""
        ...

    @overload
    def read(self, max_bytes: int) -> bytes:
        """Read up to `max_bytes` bytes."""
        ...

    def read_all(self) -> bytes:
        """Read all currently available bytes."""
        ...

    @overload
    def write(self, data: bytes) -> int:
        """Write raw bytes and return number of bytes written."""
        ...

    @overload
    def write(self, data: str) -> int:
        """Write a string (encoded as UTF-8) and return bytes written."""
        ...

    def in_waiting(self) -> int:
        """Return the number of bytes currently available to read."""
        ...

    def ifname(self) -> str:
        """Return the interface name passed at construction."""
        ...

    def scheme(self) -> str:
        """Return the parsed scheme (protocol)."""
        ...

    def is_ready(self) -> bool:
        """Return whether the underlying transport is ready for I/O."""
        ...

    def last_error(self) -> str:
        """Return the last transport error, if any."""
        ...

    def set_receive_callback(self, callback: Callable[[bytes, Dict[str, str]], None] | None) -> None:
        """Set a receive callback invoked with (data, context)."""
        ...

    def start(self) -> None:
        """Initialize the underlying transport if not started."""
        ...

    def close(self) -> None:
        """Close the transport and stop any receive thread."""
        ...

    def __enter__(self) -> UniTransmit:
        """Enter a context manager, ensuring the transport is started."""
        ...

    def __exit__(self, exc_type, exc, tb) -> None:
        """Exit a context manager, closing the transport."""
        ...
