#!/usr/bin/env python3
import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "results", "timings.csv")
OUT = os.path.join(ROOT, "results")
KERNELS = ["matmul", "heat2d", "kmeans", "map", "reduction", "sparselu", "vecmul"]

with open(CSV_PATH) as f:
    rows = list(csv.DictReader(f))
if not rows:
    sys.exit("no data in " + CSV_PATH)


def entries(kernel, impl, threads):
    return [r for r in rows
            if r["kernel"] == kernel and r["impl"] == impl and int(r["threads"]) == threads]


def best_secs(kernel, impl, threads):
    e = entries(kernel, impl, threads)
    return min(float(r["seconds"]) for r in e) if e else None


threads = sorted({int(r["threads"]) for r in rows if r["impl"] == "omp"})

seq = {k: best_secs(k, "seq", 1) for k in KERNELS}
speedup = {k: [seq[k] / best_secs(k, "omp", t) for t in threads] for k in KERNELS}
eff = {k: [s / t for s, t in zip(speedup[k], threads)] for k in KERNELS}

print("Correctness check (seq vs omp):")
lines = ["# Timing summary", ""]
for k in KERNELS:
    seq_cs = {r["checksum"] for r in entries(k, "seq", 1)}
    omp_cs = {r["checksum"] for r in rows if r["kernel"] == k and r["impl"] == "omp"}
    if k == "kmeans":
        ok = seq_cs == omp_cs
        print(f"  {k:7s} labelhash identical: {ok}")
        lines.append(f"- {k}: labelhash identical: {ok}")
    else:
        a = float(next(iter(seq_cs)))
        b = float(next(iter(omp_cs)))
        rel = abs(a - b) / max(1.0, abs(a))
        verdict = "OK" if rel < 1e-9 else "MISMATCH"
        print(f"  {k:7s} checksum rel.diff = {rel:.3e} ({verdict})")
        lines.append(f"- {k}: checksum relative difference = {rel:.3e} ({verdict})")

lines += ["", f"Best-of-N times in seconds (N=reps per config, size per kernel):", ""]
header = "| threads | " + " | ".join(KERNELS) + " |"
lines += [header, "|" + "---|" * (len(KERNELS) + 1)]
lines.append("| seq | " + " | ".join(f"{seq[k]:.4f}" for k in KERNELS) + " |")
for i, t in enumerate(threads):
    lines.append("| " + str(t) + " | " + " | ".join(f"{best_secs(k, 'omp', t):.4f}" for k in KERNELS) + " |")

lines += ["", "Speedup vs sequential:", "", header, "|" + "---|" * (len(KERNELS) + 1)]
for i, t in enumerate(threads):
    lines.append("| " + str(t) + " | " + " | ".join(f"{speedup[k][i]:.2f}x" for k in KERNELS) + " |")

lines += ["", "Parallel efficiency (speedup / threads):", "", header, "|" + "---|" * (len(KERNELS) + 1)]
for i, t in enumerate(threads):
    lines.append("| " + str(t) + " | " + " | ".join(f"{eff[k][i]:.2f}" for k in KERNELS) + " |")

with open(os.path.join(OUT, "summary.md"), "w") as f:
    f.write("\n".join(lines) + "\n")

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))
colors = dict(zip(KERNELS, plt.cm.tab10.colors))
for k in KERNELS:
    ax1.plot(threads, speedup[k], "o-", color=colors[k], label=k)
ax1.plot(threads, threads, "k--", linewidth=1, label="ideal")
ax1.set_xlabel("threads")
ax1.set_ylabel("speedup vs sequential")
ax1.set_title("Speedup")
ax1.grid(True, alpha=0.3)
ax1.legend()

for k in KERNELS:
    ax2.plot(threads, eff[k], "o-", color=colors[k], label=k)
ax2.axhline(1.0, color="k", linestyle="--", linewidth=1)
ax2.set_xlabel("threads")
ax2.set_ylabel("efficiency")
ax2.set_title("Parallel efficiency")
ax2.grid(True, alpha=0.3)
ax2.legend()

fig.tight_layout()
fig.savefig(os.path.join(OUT, "speedup.png"), dpi=150)
print("wrote", os.path.join(OUT, "summary.md"))
print("wrote", os.path.join(OUT, "speedup.png"))
