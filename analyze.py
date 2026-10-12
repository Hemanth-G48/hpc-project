#!/usr/bin/env python3
"""
analyze.py - turn gem5's stats.txt files into the numbers and the chart
we actually show in the presentation.

What we pull out, and why each one matters for the paper:

  simSeconds            how long the program took. This is the paper's
                        "execution time" in Figures 3, 7 and 9.

  CPI                   cycles per instruction. The cores run the exact
                        same instructions in every policy, so any rise in
                        CPI is the core sitting idle waiting for memory.
                        This is our version of the paper's "dispatch stall
                        cycles".

  L2 miss latency       average cycles to service a miss that went out to
                        memory. This is the paper's Figure 2 and Figure 5.

  accesses per bank     proof that the data distribution policy actually
                        moved the data where we said it did.
"""

import os
import re
import sys
import glob
import json

HERE = os.path.dirname(os.path.realpath(__file__))
RESULTS = os.path.join(HERE, "results")

# gem5's tick is 1 ps. The core clock is taken from a run's config.ini when one
# was kept, so that changing --clock in configs/numa_system.py cannot silently
# corrupt CPI; the fallback is that script's own default.
DEFAULT_TICKS_PER_CYCLE = 1e12 / 2e9


def ticks_per_cycle():
    """
    Simulator ticks per core cycle, from [system.clk_domain] in config.ini.

    gem5 writes that value in ticks, which are picoseconds, so it is the
    answer directly: 500 for the 2 GHz this project runs at.
    """
    for path in sorted(glob.glob(os.path.join(RESULTS, "*", "config.ini"))):
        section = ""
        with open(path, errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if line.startswith("["):
                    section = line
                elif section == "[system.clk_domain]" and line.startswith("clock="):
                    return float(line.split("=", 1)[1])
    return DEFAULT_TICKS_PER_CYCLE


TICKS_PER_CYCLE = ticks_per_cycle()

RUNS_A = [
    ("work_stealing_remote", "Work-stealing, data on a far node (baseline)"),
    ("fine_interleaved", "Fine distribution, pages striped over banks"),
    ("locality_aware_coarse", "Locality-aware + coarse, data on near node"),
]

RUNS_B = [
    ("dist_30ns", 30),
    ("dist_90ns", 90),
    ("dist_150ns", 150),
    ("dist_210ns", 210),
]


def read_stats(run):
    """
    Parse a gem5 stats.txt into {name: float}.

    The benchmark calls m5_reset_stats before the parallel region and
    m5_dump_stats after it, so the FIRST block in stats.txt covers
    exactly the parallel region. gem5 writes a second block when the
    program exits, which also includes teardown, and we ignore it.
    """
    path = os.path.join(RESULTS, run, "stats.txt")
    if not os.path.exists(path):
        return None
    stats = {}
    blocks = 0
    with open(path) as fh:
        for line in fh:
            if "Begin Simulation Statistics" in line:
                blocks += 1
                if blocks > 1:
                    break
                continue
            line = line.split("#")[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                stats[parts[0]] = float(parts[1])
            except ValueError:
                pass
    if blocks < 2:
        print(f"warning: {path} has only {blocks} stats block; expected 2 "
              f"(one from m5_dump_stats for the parallel region, one at exit). "
              f"Was the benchmark built with -DGEM5? Without it the numbers "
              f"below describe the whole program, first-touch pass included.",
              file=sys.stderr)
    return stats


def read_checksum(run):
    """
    Pull the checksum the benchmark prints out of the run's console.log.

    Every policy runs the identical loop, so every run must print the same
    number. run_experiments.sh tees gem5's output into console.log, so that
    is where the proof lives. Returns None if the log is not there.
    """
    path = os.path.join(RESULTS, run, "console.log")
    if not os.path.exists(path):
        return None
    with open(path, errors="replace") as fh:
        m = re.search(r"checksum=(\d+)", fh.read())
    return int(m.group(1)) if m else None


def summarise(stats, run):
    """Collapse per-core and per-bank stats into the handful we report."""
    out = {}
    out["sim_seconds"] = stats.get("simSeconds", float("nan"))
    out["sim_ticks"] = stats.get("simTicks", float("nan"))

    per_core_cycles = [v for k, v in stats.items()
                       if re.fullmatch(r"system\.cpu\d*\.numCycles", k)]
    insts = sum(v for k, v in stats.items()
                if re.fullmatch(r"system\.cpu\d*\.commitStats\d+\.numInsts", k))

    # Length of the measured region, in cycles, from simTicks: exact, and not
    # affected by the per-core counter problem described below.
    region_cycles = stats.get("simTicks", float("nan")) / TICKS_PER_CYCLE

    # SANITY CHECK. On this gem5 version cpu1..cpu3's numCycles is not cleared
    # by m5_reset_stats: it counts from tick 0, so it also contains the 112 MB
    # first-touch pass that runs *before* the region and comes out longer than
    # the region itself. Summing those counters inflates CPI by a constant
    # ~+2.0 and compresses every CPI ratio in the report. Measure the region
    # from simTicks and scale by the number of cores instead.
    if per_core_cycles and max(per_core_cycles) > region_cycles * 1.01:
        print(f"warning: {run}: a core reports more cycles than the measured "
              f"region ({max(per_core_cycles):,.0f} > {region_cycles:,.0f}); "
              f"per-core numCycles is contaminated, using simTicks",
              file=sys.stderr)

    cycles = region_cycles * len(per_core_cycles)
    out["cycles"] = cycles
    out["instructions"] = insts
    out["cpi"] = cycles / insts if insts else float("nan")

    # Average cost of an L2 miss, weighted over all cores.
    lat_total, miss_total = 0.0, 0.0
    for k, v in stats.items():
        m = re.fullmatch(r"system\.cpu(\d*)\.l2cache\.overallMissLatency::total", k)
        if m:
            lat_total += v
            miss_total += stats.get(
                f"system.cpu{m.group(1)}.l2cache.overallMisses::total", 0.0)
    out["l2_misses"] = miss_total
    out["l2_miss_latency_ticks"] = lat_total / miss_total if miss_total else 0.0

    # Where did the memory traffic actually land?
    banks = {}
    for k, v in stats.items():
        m = re.fullmatch(r"system\.mem_ctrls(\d+)\.numReads::total", k)
        if m:
            idx = int(m.group(1))
            banks[idx] = banks.get(idx, 0.0) + v
        m = re.fullmatch(r"system\.mem_ctrls(\d+)\.numWrites::total", k)
        if m:
            idx = int(m.group(1))
            banks[idx] = banks.get(idx, 0.0) + v
    out["bank_accesses"] = [banks.get(i, 0.0) for i in range(max(banks) + 1)] \
        if banks else []
    return out


def fmt_bank_split(accesses):
    total = sum(accesses) or 1.0
    return "  ".join(
        f"b{i}:{100.0 * a / total:5.1f}%" for i, a in enumerate(accesses))


def main():
    collected = {}
    for run, _ in RUNS_A + RUNS_B:
        s = read_stats(run)
        if s is None:
            print(f"warning: no stats for {run} (results/{run}/stats.txt "
                  f"missing); it is left out of the tables below",
                  file=sys.stderr)
            continue
        collected[run] = summarise(s, run)

    if not collected:
        print(f"no stats found under {RESULTS}, run run_experiments.sh first")
        return 1

    lines = []
    add = lines.append

    add("=" * 78)
    add("EXPERIMENT A: the paper's data distribution and scheduling policies")
    add("=" * 78)
    add("")
    add("Same kernel, same number of instructions in every row.")
    add("The only difference is which memory bank serves the data.")
    add("")
    header = (f"{'policy':<48}{'time (ms)':>11}{'CPI':>8}"
              f"{'speedup':>9}")
    add(header)
    add("-" * len(header))

    base = collected.get("work_stealing_remote", {}).get("sim_ticks")
    for run, label in RUNS_A:
        if run not in collected:
            continue
        c = collected[run]
        sp = base / c["sim_ticks"] if base and c["sim_ticks"] else float("nan")
        add(f"{label:<48}{c['sim_seconds'] * 1e3:>11.3f}"
            f"{c['cpi']:>8.2f}{sp:>8.2f}x")
    add("")
    add("Speedup is measured against the work-stealing row, which is the"
        " baseline the paper uses.")
    add("")

    add("Where the memory traffic actually went (proof the placement worked):")
    add("")
    for run, label in RUNS_A:
        if run not in collected:
            continue
        add(f"  {label:<48}{fmt_bank_split(collected[run]['bank_accesses'])}")
    add("")

    add("Average cost of one L2 miss, in ticks (1 tick = 1 ps):")
    add("")
    for run, label in RUNS_A:
        if run not in collected:
            continue
        c = collected[run]
        add(f"  {label:<48}{c['l2_miss_latency_ticks']:>12,.0f}"
            f"   ({c['l2_misses']:,.0f} misses)")
    add("")

    add("The kernel's own checksum, printed by every run (must be identical):")
    add("")
    sums = [(label, read_checksum(run)) for run, label in RUNS_A
            if run in collected]
    if all(v is None for _, v in sums):
        add("  (no console.log archived under results/, so this check was"
            " skipped)")
    else:
        for label, v in sums:
            shown = f"{v:,}" if v is not None else "missing"
            add(f"  {label:<48}{shown:>20}")
        add("")
        found = {v for _, v in sums if v is not None}
        if len(found) == 1:
            add("  All runs printed the same checksum: identical work,"
                " confirmed.")
        else:
            print(f"warning: runs printed different checksums: {sorted(found)}",
                  file=sys.stderr)
            add(f"  WARNING: runs printed different checksums: {sorted(found)}")
    add("")

    if any(r in collected for r, _ in RUNS_B):
        add("=" * 78)
        add("EXPERIMENT B: NUMA distance sweep (our version of Figure 2)")
        add("=" * 78)
        add("")
        add("Identical program, identical data placement. We only change how"
            " far away")
        add("the bank holding the data is. Everything that moves here is"
            " pure NUMA cost.")
        add("")
        h2 = f"{'bank latency':>14}{'time (ms)':>12}{'CPI':>8}{'slowdown':>11}"
        add(h2)
        add("-" * len(h2))
        near = collected.get("dist_30ns", {}).get("sim_ticks")
        for run, ns in RUNS_B:
            if run not in collected:
                continue
            c = collected[run]
            sd = c["sim_ticks"] / near if near else float("nan")
            add(f"{str(ns) + ' ns':>14}{c['sim_seconds'] * 1e3:>12.3f}"
                f"{c['cpi']:>8.2f}{sd:>10.2f}x")
        add("")

    text = "\n".join(lines)
    print(text)

    with open(os.path.join(RESULTS, "summary.txt"), "w") as fh:
        fh.write(text + "\n")
    with open(os.path.join(RESULTS, "summary.json"), "w") as fh:
        json.dump(collected, fh, indent=2)
    print(f"\nwritten: {RESULTS}/summary.txt and summary.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
