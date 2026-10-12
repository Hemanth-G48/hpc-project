#!/usr/bin/env python3
"""
make_charts.py - draw the two figures for the presentation from
results/summary.json. Writes PNGs into results/ ready to drop into slides.
"""

import json
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.realpath(__file__))
RESULTS = os.path.join(HERE, "results")

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#dedcd6"

# Validated categorical palette, worst to best.
C_BASE = "#eb6834"   # work-stealing, far node
C_FINE = "#1baf7a"   # fine distribution
C_LOCAL = "#2a78d6"  # locality-aware + coarse

with open(os.path.join(RESULTS, "summary.json")) as fh:
    S = json.load(fh)

plt.rcParams.update({
    "font.size": 11,
    "font.family": "DejaVu Sans",
    "axes.edgecolor": GRID,
    "axes.labelcolor": INK_2,
    "text.color": INK,
    "xtick.color": INK_2,
    "ytick.color": INK_2,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
})


def tidy(ax, xgrid=True):
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    ax.spines["left"].set_color(GRID)
    ax.spines["bottom"].set_color(GRID)
    if xgrid:
        ax.xaxis.grid(True, color=GRID, linewidth=0.8)
        ax.set_axisbelow(True)


# --------------------------------------------------------------------
# Labels and colours shared by the figures: worst placement first,
# best last, so every figure reads in the same order.
# --------------------------------------------------------------------
order = ["work_stealing_remote", "fine_interleaved", "locality_aware_coarse"]
labels = [
    "Work-stealing\ndata on a far node",
    "Fine distribution\npages striped over banks",
    "Locality-aware + coarse\ndata on the near node",
]
colors = [C_BASE, C_FINE, C_LOCAL]

# --------------------------------------------------------------------
# Figure 0: where each policy actually puts the data
# --------------------------------------------------------------------
# Share of one thread's 2 MB that each bank serves, under each policy. This
# is the same thing the bank table in the README reports as percentages.
share = {
    "work_stealing_remote": [0, 0, 0, 1],
    "fine_interleaved": [0.25, 0.25, 0.25, 0.25],
    "locality_aware_coarse": [1, 0, 0, 0],
}

fig, ax = plt.subplots(figsize=(9.6, 4.4))

CELL, BH, GAP = 0.84, 0.42, 0.012      # cell width, block height, block gap
for row, (key, colour) in enumerate(zip(order, colors)):
    y0 = len(order) - 1 - row
    for bank, part in enumerate(share[key]):
        if part:
            # Four blocks per bank, one per thread, filling `part` of the cell.
            filled = part * CELL
            bw = (filled - 3 * GAP) / 4
            for t in range(4):
                ax.add_patch(Rectangle((bank + 0.08 + t * (bw + GAP), y0 + 0.29),
                                       bw, BH, facecolor=colour, edgecolor="none"))
            ax.text(bank + 0.5, y0 + 0.79, f"{part * 100:.0f}%", ha="center",
                    va="center", fontsize=9.5, color=colour)
for row in range(len(order)):
    y0 = len(order) - 1 - row
    for bank in range(4):
        ax.add_patch(Rectangle((bank + 0.08, y0 + 0.29), CELL, BH,
                               facecolor="none", edgecolor=GRID, linewidth=1))

ax.set_xlim(0, 4)
ax.set_ylim(0, len(order) + 0.15)
ax.set_xticks([i + 0.5 for i in range(4)],
              ["bank 0\n30 ns\n(near)", "bank 1\n90 ns",
               "bank 2\n150 ns", "bank 3\n210 ns\n(far)"], fontsize=10)
ax.set_yticks([len(order) - 1 - r + 0.5 for r in range(len(order))],
              labels, fontsize=10)
for side in ("top", "right", "left", "bottom"):
    ax.spines[side].set_visible(False)
ax.tick_params(length=0)
ax.set_title("Where the data actually lives", loc="left", fontsize=13,
             color=INK, pad=12)
fig.text(0.012, 0.02,
         "Each thread walks 2 MB, and every block above is one thread's share. "
         "The same work in all three rows; only the bank serving it changes.",
         fontsize=9.5, color=INK_2, ha="left")
fig.tight_layout(rect=[0, 0.055, 1, 1])
fig.savefig(os.path.join(RESULTS, "fig0_placements.png"), dpi=170)
plt.close(fig)

# --------------------------------------------------------------------
# Figure 1: the three policies
# --------------------------------------------------------------------

times = [S[k]["sim_seconds"] * 1e3 for k in order]
ticks = [S[k]["sim_ticks"] for k in order]          # exact, for the ratios
miss = [S[k]["l2_miss_latency_ticks"] / 1000.0 for k in order]  # ticks -> ns

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 4.4))
y = range(len(order))

bars = ax1.barh(y, times, color=colors, height=0.55)
ax1.set_yticks(list(y), labels, fontsize=10)
ax1.invert_yaxis()
ax1.set_xlabel("execution time of the parallel region (ms)")
ax1.set_title("Same work, different data placement",
              loc="left", fontsize=13, color=INK, pad=12)
ax1.set_xlim(0, max(times) * 1.25)
for b, v, t, base in zip(bars, times, ticks, [ticks[0]] * 3):
    ax1.text(v + max(times) * 0.02, b.get_y() + b.get_height() / 2,
             f"{v:.2f} ms   ({base / t:.2f}x)",
             va="center", fontsize=10, color=INK)
tidy(ax1)

bars = ax2.barh(y, miss, color=colors, height=0.55)
ax2.set_yticks([])
ax2.invert_yaxis()
ax2.set_xlabel("average cost of one L2 miss (ns)")
ax2.set_title("Why: the cost of going to memory",
              loc="left", fontsize=13, color=INK, pad=12)
ax2.set_xlim(0, max(miss) * 1.25)
for b, v in zip(bars, miss):
    ax2.text(v + max(miss) * 0.02, b.get_y() + b.get_height() / 2,
             f"{v:.0f} ns", va="center", fontsize=10, color=INK)
tidy(ax2)

fig.suptitle(
    "Locality-aware scheduling vs work-stealing, map kernel on a "
    "4-node NUMA machine in gem5",
    fontsize=14, color=INK, x=0.012, ha="left", y=0.99)
misses = [int(S[k]["l2_misses"]) for k in order]
spread = max(misses) - min(misses)
claim = (f"the identical {max(misses):,} L2 misses" if spread == 0
         else f"the same {max(misses):,} L2 misses, give or take {spread}")
fig.text(0.012, 0.015,
         f"Every bar runs the identical kernel and takes {claim}. "
         f"Only the memory bank serving them changes.",
         fontsize=9.5, color=INK_2, ha="left")
fig.tight_layout(rect=[0, 0.045, 1, 0.93])
fig.savefig(os.path.join(RESULTS, "fig1_policies.png"), dpi=170)
plt.close(fig)

# --------------------------------------------------------------------
# Figure 2: NUMA distance sweep
# --------------------------------------------------------------------
sweep = [("dist_30ns", 30), ("dist_90ns", 90),
         ("dist_150ns", 150), ("dist_210ns", 210)]
xs = [ns for _, ns in sweep if _ in S]
ys = [S[k]["sim_seconds"] * 1e3 for k, _ in sweep if k in S]

fig, ax = plt.subplots(figsize=(7.6, 4.6))
ax.plot(xs, ys, color=C_LOCAL, linewidth=2,
        marker="o", markersize=8, markerfacecolor=C_LOCAL,
        markeredgecolor=SURFACE, markeredgewidth=2)
for x, v in zip(xs, ys):
    ax.annotate(f"{v:.2f} ms", (x, v), textcoords="offset points",
                xytext=(0, 11), ha="center", fontsize=10, color=INK)
ax.set_xlabel("latency of the memory bank holding the data (ns)")
ax.set_ylabel("execution time (ms)")
ax.set_title("The NUMA penalty, measured",
             loc="left", fontsize=13, color=INK, pad=12)
ax.set_xticks(xs)
ax.set_ylim(0, max(ys) * 1.22)
ax.set_xlim(min(xs) - 20, max(xs) + 20)
tidy(ax, xgrid=False)
ax.yaxis.grid(True, color=GRID, linewidth=0.8)
ax.set_axisbelow(True)
fig.text(0.012, 0.015,
         "Identical program, identical data placement. Only the distance "
         "to the bank changes.\nThis is our version of Figure 2 of the paper.",
         fontsize=9.5, color=INK_2, ha="left")
fig.tight_layout(rect=[0, 0.1, 1, 1])
fig.savefig(os.path.join(RESULTS, "fig2_numa_distance.png"), dpi=170)
plt.close(fig)

print("wrote results/fig0_placements.png, results/fig1_policies.png and "
      "results/fig2_numa_distance.png")
