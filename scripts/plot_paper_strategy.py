#!/usr/bin/env python3
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "results", "paper_strategy", "strategy.csv")
OUT = os.path.join(ROOT, "results", "paper_strategy")
BENCHMARKS = ["map", "vecmul", "reduction", "matmul"]

with open(CSV_PATH) as f:
    rows = list(csv.DictReader(f))


def config_label(r):
    if r["impl"] == "omp":
        return "omp"
    base = ("std" if r["sched"] == "standard" else "loc") + "/" + r["dist"]
    if r["vicinity"] != "0":
        base += "/v" + r["vicinity"]
    return base


best = {}
checksums = {}
for r in rows:
    key = (r["benchmark"], config_label(r))
    t = float(r["seconds"])
    if key not in best or t < best[key]:
        best[key] = t
    checksums.setdefault(r["benchmark"], set()).add(r["checksum"])

lines = ["# Paper-strategy study — comparative results", ""]
lines.append("All timing values are best-of-N seconds. `loc` = locality-aware scheduler (work-dealing +")
lines.append("vicinity work-stealing), `std` = standard scheduler (creator-queue + global stealing like")
lines.append("the work-stealing baseline). `coarse`/`fine` = data distribution policy passed to `ps_alloc`.")
lines.append("")

print("Checksum verification (identical results across all schedulers/policies/OpenMP):")
for b in BENCHMARKS:
    ok = len(checksums.get(b, ())) == 1
    print(f"  {b:10s} identical: {ok}")
    lines.append(f"- **{b}** identical checksums across all configs: {ok}")

labels = [("omp", None),
          ("std/coarse", None),
          ("loc/coarse", None),
          ("loc/coarse/v1", None),
          ("loc/coarse/v2", None),
          ("loc/fine", None)]

fig, axes = plt.subplots(1, len(BENCHMARKS), figsize=(4.2 * len(BENCHMARKS), 4))
for ax, b in zip(axes, BENCHMARKS):
    present = [lbl for lbl, _ in labels if (b, lbl) in best]
    base = best[(b, "omp")]
    vals = [best[(b, lbl)] / base for lbl in present]
    ax.bar(range(len(present)), vals, color="tab:blue")
    ax.set_xticks(range(len(present)))
    ax.set_xticklabels(present, rotation=35, ha="right", fontsize=8)
    ax.axhline(1.0, color="k", linestyle="--", linewidth=1)
    ax.set_title(b)
    ax.set_ylabel("time / omp time")
    ax.grid(True, axis="y", alpha=0.3)
fig.tight_layout()
fig.savefig(os.path.join(OUT, "strategy.png"), dpi=150)

lines += ["", "Best-of-N times (s), normalized to plain OpenMP:", "",
          "| benchmark | " + " | ".join(lbl for lbl, _ in labels if any((b, lbl) in best for b in BENCHMARKS)) + " |",
          "|" + "---|" * (len(labels) + 2)]
for b in BENCHMARKS:
    cells = []
    for lbl, _ in labels:
        if (b, lbl) in best:
            cells.append(f"{best[(b, lbl)]:.4f} ({best[(b, lbl)] / best[(b, 'omp')]:.2f}x)")
        else:
            cells.append("—")
    lines.append(f"| {b} | " + " | ".join(cells) + " |")

lines += ["", "Plot: `strategy.png`. Raw data: `strategy.csv`, scheduler stats: `stats.log`."]

with open(os.path.join(OUT, "summary.md"), "w") as f:
    f.write("\n".join(lines) + "\n")

print("wrote", os.path.join(OUT, "summary.md"))
print("wrote", os.path.join(OUT, "strategy.png"))
