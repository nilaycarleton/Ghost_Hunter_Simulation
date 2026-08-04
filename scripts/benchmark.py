#!/usr/bin/env python3
"""Generate reproducible BFS, breadcrumb, and random navigation benchmarks."""
import csv
import json
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "benchmarks"
SEEDS = tuple(range(1, 21))
STRATEGIES = ("bfs", "breadcrumb", "random")


def run(strategy, seed):
    with tempfile.NamedTemporaryFile(suffix=".json") as summary:
        started = time.perf_counter()
        subprocess.run(
            [ROOT / "ghost_hunt", "--seed", str(seed), "--navigation", strategy,
             "--deterministic", "--tick-ms", "0", "--max-ticks", "500",
             "--hunters", "Ada,Grace,Linus", "--output-json", summary.name],
            cwd=ROOT, stdout=subprocess.DEVNULL, check=True, timeout=3,
        )
        elapsed = time.perf_counter() - started
        result = json.load(summary)
    hunter_moves = sum(hunter["moves"] for hunter in result["hunters"])
    hunter_ticks = sum(hunter["ticks"] for hunter in result["hunters"])
    evidence_found = sum(hunter["evidence_found"] for hunter in result["hunters"])
    return {
        "strategy": strategy,
        "seed": seed,
        "elapsed_seconds": round(elapsed, 6),
        "hunter_moves": hunter_moves,
        "hunter_ticks": hunter_ticks,
        "evidence_found": evidence_found,
        "solved": int(result["solved"]),
        "timeouts": sum(hunter["exit"] == "timeout" for hunter in result["hunters"]),
    }


def summarize(rows):
    return {
        strategy: {
            "moves": sum(r["hunter_moves"] for r in rows if r["strategy"] == strategy) / len(SEEDS),
            "ticks": sum(r["hunter_ticks"] for r in rows if r["strategy"] == strategy) / len(SEEDS),
            "solved": 100 * sum(r["solved"] for r in rows if r["strategy"] == strategy) / len(SEEDS),
            "evidence": sum(r["evidence_found"] for r in rows if r["strategy"] == strategy) / len(SEEDS),
            "timeouts": sum(r["timeouts"] for r in rows if r["strategy"] == strategy),
        }
        for strategy in STRATEGIES
    }


def svg(summary):
    maximum = max(item["moves"] for item in summary.values()) or 1
    bars = []
    for index, (name, metrics) in enumerate(summary.items()):
        width = 400 * metrics["moves"] / maximum
        y = 92 + index * 76
        bars.append(
            f'<text x="30" y="{y + 25}" fill="#e9ecdf">{name}</text>'
            f'<rect x="150" y="{y}" width="{width:.1f}" height="36" fill="#c8ff4d"/>'
            f'<text x="{165 + width:.1f}" y="{y + 24}" fill="#e9ecdf">'
            f'{metrics["moves"]:.1f} moves · {metrics["solved"]:.0f}% solved</text>'
        )
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="760" height="360">
<rect width="100%" height="100%" fill="#0b0f0e"/>
<text x="30" y="40" fill="#e9ecdf" font-size="22" font-family="monospace">Navigation strategy comparison</text>
<text x="30" y="64" fill="#899185" font-size="12" font-family="monospace">20 seeded deterministic runs · mean hunter moves · lower is better</text>
<g font-family="monospace" font-size="14">{''.join(bars)}</g></svg>"""


def main():
    subprocess.run(["make"], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    rows = [run(strategy, seed) for strategy in STRATEGIES for seed in SEEDS]
    summary = summarize(rows)
    OUT.mkdir(exist_ok=True)
    with (OUT / "navigation.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    with (OUT / "navigation-summary.json").open("w") as output:
        json.dump(summary, output, indent=2)
        output.write("\n")
    (OUT / "navigation.svg").write_text(svg(summary))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
