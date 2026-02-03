"""Unified transport interface for multiple protocols."""

from __future__ import annotations

import os
import sys

try:
    from ._unitransmit import UniTransmit
except ModuleNotFoundError as exc:
    build_dir = os.environ.get("UNITRANSMIT_BUILD_DIR", "build")
    if os.path.isdir(build_dir):
        sys.path.insert(0, build_dir)
        try:
            from ._unitransmit import UniTransmit
        except ModuleNotFoundError:
            raise ModuleNotFoundError(
                "unitransmit._unitransmit not found. "
                "Build the extension (e.g. `cmake --build build` or `python -m build`) "
                "and run outside the repo root or install the wheel."
            ) from exc
        finally:
            if sys.path and sys.path[0] == build_dir:
                sys.path.pop(0)
    else:
        raise ModuleNotFoundError(
            "unitransmit._unitransmit not found. "
            "Build the extension (e.g. `cmake --build build` or `python -m build`) "
            "and run outside the repo root or install the wheel."
        ) from exc

__all__ = ["UniTransmit"]
