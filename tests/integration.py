#!/usr/bin/env python3
"""End-to-end checks for deterministic replay, summaries, and event ordering."""
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "ghost_hunt"


def deterministic_summary(path):
    result = subprocess.run(
        [BINARY, "--seed", "4242", "--deterministic", "--tick-ms", "0",
         "--navigation", "bfs", "--hunters", "Ada,Grace",
         "--output-json", path],
        cwd=ROOT, text=True, capture_output=True, check=True, timeout=3,
        env={**os.environ, "GH_JSON_EVENTS": "1"},
    )
    events = [
        json.loads(line.removeprefix("EVENT "))
        for line in result.stdout.splitlines() if line.startswith("EVENT ")
    ]
    assert events
    assert [event["sequence"] for event in events] == list(range(1, len(events) + 1))
    assert all(isinstance(event["timestamp_us"], int) for event in events)
    return Path(path).read_bytes()


def main():
    with tempfile.TemporaryDirectory() as directory:
        first = Path(directory) / "first.json"
        second = Path(directory) / "second.json"
        assert deterministic_summary(first) == deterministic_summary(second)
        summary = json.loads(first.read_text())
        assert summary["scheduler"] == "deterministic"
        assert summary["navigation"] == "bfs"
        assert summary["seed"] == 4242
        assert len(summary["hunters"]) == 2
    print("Integration tests passed: deterministic replay, JSON summary, ordered events.")


if __name__ == "__main__":
    main()
