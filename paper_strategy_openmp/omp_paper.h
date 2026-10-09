#ifndef OMP_PAPER_H
#define OMP_PAPER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Seam for the paper's data-distribution policies inside OpenMP.
 *
 * Stock OpenMP has no memory-placement API: there is no way to ask for
 * "fine" (interleaved) or "coarse" (per-allocation) placement. Those policies
 * are provided by OS/runtime facilities instead (libnuma's mbind /
 * numa_alloc_interleaved on NUMA systems; first-touch initialization from
 * bound threads). op_alloc() is the place where that call would go; here it
 * records the requested policy as metadata and reports it in OPSTATS.
 */

typedef enum { OP_STANDARD = 0, OP_FINE = 1, OP_COARSE = 2 } op_policy_t;

static struct {
    op_policy_t policy;
    int nallocs;
    size_t bytes;
    int stats;
} op_state;

static inline void op_init(void)
{
    const char *s = getenv("OP_DIST");
    op_state.policy = OP_STANDARD;
    if (s && strcmp(s, "fine") == 0)
        op_state.policy = OP_FINE;
    if (s && strcmp(s, "coarse") == 0)
        op_state.policy = OP_COARSE;
    op_state.stats = getenv("OP_STATS") != NULL;
    op_state.nallocs = 0;
    op_state.bytes = 0;
}

static inline void *op_alloc(size_t size)
{
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "op_alloc(%zu) failed\n", size);
        exit(1);
    }
    op_state.nallocs++;
    op_state.bytes += size;
    return p;
}

static inline const char *op_dist_name(void)
{
    return op_state.policy == OP_COARSE ? "coarse"
         : op_state.policy == OP_FINE ? "fine" : "standard";
}

static inline void op_stats(void)
{
    if (op_state.stats)
        fprintf(stderr, "OPSTATS dist=%s allocs=%d bytes=%zu\n",
                op_dist_name(), op_state.nallocs, op_state.bytes);
}

#endif
