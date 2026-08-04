#!/usr/bin/env python3
"""Fast deterministic-seed stress test for hangs, crashes, and invalid CLI."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "ghost_hunt"
RUNS = 200


def main():
    for seed in range(RUNS):
        strategy = ("bfs", "breadcrumb", "random")[seed % 3]
        result = subprocess.run(
            [BINARY, "--seed", str(seed), "--navigation", strategy,
             "--tick-ms", "0", "--max-ticks", "500",
             "--hunters", "Ada,Grace,Linus",
             *(["--deterministic"] if seed % 2 == 0 else [])],
            cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            timeout=3,
        )
        if result.returncode:
            print(f"seed {seed} failed: {result.stderr.decode()}", file=sys.stderr)
            return 1

    invalid_cases = (
        ["--seed", "-1"],
        ["--navigation", "teleport"],
        ["--tick-ms", "10001"],
        ["--max-ticks", "0"],
        ["--hunters", ""],
    )
    for arguments in invalid_cases:
        result = subprocess.run(
            [BINARY, *arguments], cwd=ROOT,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        if result.returncode != 2:
            print(f"invalid arguments accepted: {arguments}", file=sys.stderr)
            return 1

    print(f"Stress test passed: {RUNS} seeded runs and {len(invalid_cases)} invalid inputs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
