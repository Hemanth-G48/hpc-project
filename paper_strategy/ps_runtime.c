#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ps_runtime.h"

#define PS_MAX_LOC 32
#define PS_MAX_ALLOCS 256
#define PS_MAX_DEPS 8

typedef struct ps_task {
    void (*fn)(void *);
    void *arg;
    int dom;
    struct ps_task *next;
} ps_task_t;

typedef struct {
    ps_task_t *head;
    ps_task_t *tail;
    int count;
    pthread_mutex_t m;
} ps_queue_t;

typedef struct {
    const void *base;
    size_t size;
    ps_policy_t policy;
    int home;
} ps_alloc_t;

static struct {
    int nloc;
    int nthreads;
    int tpl;
    ps_sched_t sched;
    ps_policy_t policy;
    int vicinity;
    long cutoff;
    int dist[PS_MAX_LOC][PS_MAX_LOC];
    ps_queue_t q[PS_MAX_LOC];
    ps_alloc_t allocs[PS_MAX_ALLOCS];
    int nallocs;
    size_t rr;
    pthread_mutex_t glock;
    pthread_cond_t gcv;
    long pending;
    int running;
    int stats;
    pthread_t th[64];
    long executed[PS_MAX_LOC];
    long dealt[PS_MAX_LOC];
    long steals[PS_MAX_LOC];
    long foreign[PS_MAX_LOC];
} ps;

static __thread int tl_loc;

const char *ps_sched_name(void)
{
    return ps.sched == PS_SCHED_LOCALITY ? "locality" : "standard";
}

const char *ps_dist_name(void)
{
    return ps.policy == PS_COARSE ? "coarse"
         : ps.policy == PS_FINE ? "fine" : "standard";
}

ps_policy_t ps_env_policy(void)
{
    const char *s = getenv("PS_DIST");
    if (s && strcmp(s, "fine") == 0)
        return PS_FINE;
    if (s && strcmp(s, "coarse") == 0)
        return PS_COARSE;
    return PS_STANDARD;
}

static void q_push(ps_queue_t *q, ps_task_t *t)
{
    pthread_mutex_lock(&q->m);
    t->next = NULL;
    if (q->tail)
        q->tail->next = t;
    else
        q->head = t;
    q->tail = t;
    q->count++;
    pthread_mutex_unlock(&q->m);
}

static ps_task_t *q_pop(ps_queue_t *q)
{
    pthread_mutex_lock(&q->m);
    ps_task_t *t = q->head;
    if (t) {
        q->head = t->next;
        if (!q->head)
            q->tail = NULL;
        q->count--;
    }
    pthread_mutex_unlock(&q->m);
    return t;
}

static long order_cost(int cand, const ps_dep_t *deps, int ndeps)
{
    double c = 0.0;
    for (int i = 0; i < ndeps; i++) {
        int ai = deps[i].aid;
        if (ai < 0 || ai >= ps.nallocs || ps.allocs[ai].policy != PS_COARSE)
            continue;
        c += (double)deps[i].len * (double)ps.dist[cand][ps.allocs[ai].home];
    }
    return (long)c;
}

static ps_task_t *try_steal(int loc)
{
    int n = ps.nloc;
    for (int step = 1; step < n; step++) {
        if (ps.vicinity > 0) {
            int rd = step < n - step ? step : n - step;
            if (rd > ps.vicinity)
                continue;
        }
        int v = (loc + step) % n;
        if (ps.q[v].count <= 1)
            continue;
        ps_task_t *t = q_pop(&ps.q[v]);
        if (t) {
            pthread_mutex_lock(&ps.glock);
            ps.steals[loc]++;
            pthread_mutex_unlock(&ps.glock);
            return t;
        }
    }
    return NULL;
}

static int pick_cpu(int k)
{
    int ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    /* Hybrid layout: 6 P-cores with SMT (cpus 0-11) + 4 E-cores (cpus 12-15).
       Spread workers over physical cores first, then SMT siblings, then E-cores. */
    if (ncpu == 16) {
        if (k < 6)
            return 2 * k;
        if (k < 12)
            return 2 * (k - 6) + 1;
        return 12 + (k - 12);
    }
    return k % ncpu;
}

static void *worker(void *arg)
{
    int me = (int)(long)arg;
    int loc = me / ps.tpl;
    tl_loc = loc;

    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(pick_cpu(me), &cs);
    pthread_setaffinity_np(pthread_self(), sizeof cs, &cs);

    long idle = 0;
    for (;;) {
        ps_task_t *t = q_pop(&ps.q[loc]);
        if (!t)
            t = try_steal(loc);
        if (t) {
            t->fn(t->arg);
            pthread_mutex_lock(&ps.glock);
            ps.executed[loc]++;
            if (t->dom >= 0 && loc != t->dom)
                ps.foreign[loc]++;
            if (--ps.pending == 0)
                pthread_cond_broadcast(&ps.gcv);
            pthread_mutex_unlock(&ps.glock);
            free(t);
            idle = 0;
            continue;
        }

        pthread_mutex_lock(&ps.glock);
        if (!ps.running) {
            pthread_mutex_unlock(&ps.glock);
            break;
        }
        long pend = ps.pending;
        pthread_mutex_unlock(&ps.glock);

        if (pend > 0 || idle++ < 4096) {
            __asm__ __volatile__("pause" ::: "memory");
            continue;
        }

        pthread_mutex_lock(&ps.glock);
        if (ps.running && ps.pending == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&ps.gcv, &ps.glock, &ts);
        }
        pthread_mutex_unlock(&ps.glock);
    }
    return NULL;
}

void ps_init(void)
{
    if (ps.running)
        return;

    const char *s;
    int ncpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    ps.nthreads = ncpu < 12 ? ncpu : 12;
    if ((s = getenv("PS_THREADS")))
        ps.nthreads = atoi(s);
    if (ps.nthreads > 64)
        ps.nthreads = 64;
    ps.nloc = ps.nthreads;
    if ((s = getenv("PS_LOCATIONS")))
        ps.nloc = atoi(s);
    if (ps.nloc > PS_MAX_LOC)
        ps.nloc = PS_MAX_LOC;
    if (ps.nloc < 1)
        ps.nloc = 1;
    ps.tpl = ps.nthreads / ps.nloc;
    if (ps.tpl < 1)
        ps.tpl = 1;
    ps.nthreads = ps.nloc * ps.tpl;

    ps.sched = (getenv("PS_SCHED") && strcmp(getenv("PS_SCHED"), "locality") == 0)
                   ? PS_SCHED_LOCALITY : PS_SCHED_STANDARD;
    ps.policy = ps_env_policy();
    ps.vicinity = (s = getenv("PS_VICINITY")) ? atoi(s) : 0;
    ps.cutoff = (s = getenv("PS_CUTOFF")) ? atol(s) : 8192;
    ps.stats = getenv("PS_STATS") != NULL;

    for (int i = 0; i < ps.nloc; i++)
        for (int j = 0; j < ps.nloc; j++)
            ps.dist[i][j] = (i == j) ? 10 : 21;

    for (int i = 0; i < ps.nloc; i++) {
        ps.q[i].head = ps.q[i].tail = NULL;
        ps.q[i].count = 0;
        pthread_mutex_init(&ps.q[i].m, NULL);
    }
    pthread_mutex_init(&ps.glock, NULL);
    pthread_cond_init(&ps.gcv, NULL);

    ps.running = 1;
    for (int k = 0; k < ps.nthreads; k++)
        pthread_create(&ps.th[k], NULL, worker, (void *)(long)k);

    tl_loc = 0;
    atexit(ps_shutdown);
}

void ps_shutdown(void)
{
    if (!ps.running)
        return;
    pthread_mutex_lock(&ps.glock);
    ps.running = 0;
    pthread_cond_broadcast(&ps.gcv);
    pthread_mutex_unlock(&ps.glock);
    for (int k = 0; k < ps.nthreads; k++)
        pthread_join(ps.th[k], NULL);

    if (ps.stats) {
        fprintf(stderr, "PSSTATS sched=%s dist=%s locs=%d threads=%d\n",
                ps_sched_name(), ps_dist_name(), ps.nloc, ps.nthreads);
        for (int i = 0; i < ps.nloc; i++)
            fprintf(stderr, "PSSTATS loc=%d executed=%ld dealt=%ld steals=%ld foreign=%ld\n",
                    i, ps.executed[i], ps.dealt[i], ps.steals[i], ps.foreign[i]);
    }
}

void *ps_ptr(int aid)
{
    return (void *)ps.allocs[aid].base;
}

int ps_alloc(size_t size, ps_policy_t policy)
{
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "ps_alloc(%zu) failed\n", size);
        exit(1);
    }
    if (ps.nallocs >= PS_MAX_ALLOCS) {
        fprintf(stderr, "ps_alloc: too many allocations\n");
        exit(1);
    }
    int id = ps.nallocs++;
    ps.allocs[id].base = p;
    ps.allocs[id].size = size;
    ps.allocs[id].policy = policy;
    ps.allocs[id].home = (int)(ps.rr++ % (size_t)ps.nloc);
    return id;
}

void ps_free(void *p)
{
    free(p);
}

void ps_task(void (*fn)(void *), void *arg, const ps_dep_t *deps, int ndeps)
{
    ps_task_t *t = malloc(sizeof *t);
    if (!t) {
        fprintf(stderr, "ps_task: out of memory\n");
        exit(1);
    }
    t->fn = fn;
    t->arg = arg;
    t->next = NULL;
    if (ndeps > PS_MAX_DEPS)
        ndeps = PS_MAX_DEPS;

    int dom = -1;
    int hint_home = -1;
    double best_int = 0.0;
    size_t coarse = 0;
    size_t dom_len = 0;
    int spread = 0;

    for (int i = 0; i < ndeps; i++) {
        int ai = deps[i].aid;
        if (ai < 0 || ai >= ps.nallocs || ps.allocs[ai].policy != PS_COARSE) {
            spread = 1;
            continue;
        }
        coarse += deps[i].len;
        if (deps[i].intensity > best_int) {
            best_int = deps[i].intensity;
            hint_home = ps.allocs[ai].home;
        }
        if (deps[i].len > dom_len) {
            dom_len = deps[i].len;
            dom = ps.allocs[ai].home;
        }
    }
    t->dom = dom;

    int target;
    if (ps.sched == PS_SCHED_STANDARD || spread || coarse < (size_t)ps.cutoff) {
        target = tl_loc;
    } else if (hint_home >= 0) {
        target = hint_home;
    } else {
        target = dom >= 0 ? dom : tl_loc;
        long best = order_cost(target, deps, ndeps);
        for (int c = 0; c < ps.nloc; c++) {
            if (c == target)
                continue;
            long cc = order_cost(c, deps, ndeps);
            if (cc < best) {
                best = cc;
                target = c;
            }
        }
    }

    q_push(&ps.q[target], t);
    pthread_mutex_lock(&ps.glock);
    long was = ps.pending;
    ps.pending++;
    ps.dealt[target]++;
    if (was == 0)
        pthread_cond_broadcast(&ps.gcv);
    pthread_mutex_unlock(&ps.glock);
}

void ps_wait(void)
{
    pthread_mutex_lock(&ps.glock);
    while (ps.pending > 0)
        pthread_cond_wait(&ps.gcv, &ps.glock);
    pthread_mutex_unlock(&ps.glock);
}

int ps_nlocations(void)
{
    return ps.nloc;
}

int ps_nthreads(void)
{
    return ps.nthreads;
}

int ps_current_location(void)
{
    return tl_loc;
}
