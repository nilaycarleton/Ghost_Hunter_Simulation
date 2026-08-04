# Navigation benchmark methodology

The benchmark runs BFS, breadcrumb-stack, and random-walk return navigation
against the same 20 seeds. It uses the deterministic scheduler, three hunters,
zero wall-clock delay, and a 500-tick ceiling.

```bash
python3 scripts/benchmark.py
```

| Strategy | Mean hunter moves | Mean hunter ticks | Timeouts |
|---|---:|---:|---:|
| BFS | 27.0 | 30.0 | 0 |
| Breadcrumb | 41.6 | 44.65 | 0 |
| Random | 92.7 | 95.7 | 0 |

BFS required approximately 35% fewer movements than breadcrumb navigation and
71% fewer than random walking for these seeds. The raw observations are in
`navigation.csv`; aggregated values are in `navigation-summary.json`.

The solved-case rate is also recorded, but it is not treated as a navigation
performance metric because evidence availability, device assignment, and ghost
behavior dominate that outcome.
