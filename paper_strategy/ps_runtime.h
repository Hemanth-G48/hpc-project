#ifndef PS_RUNTIME_H
#define PS_RUNTIME_H

#include <stddef.h>

typedef enum { PS_STANDARD = 0, PS_FINE = 1, PS_COARSE = 2 } ps_policy_t;
typedef enum { PS_SCHED_STANDARD = 0, PS_SCHED_LOCALITY = 1 } ps_sched_t;

typedef struct {
    int aid;
    size_t off;
    size_t len;
    double intensity;
} ps_dep_t;

void ps_init(void);
void ps_shutdown(void);

int ps_alloc(size_t size, ps_policy_t policy);
void *ps_ptr(int aid);
void ps_free(void *p);

void ps_task(void (*fn)(void *), void *arg, const ps_dep_t *deps, int ndeps);
void ps_wait(void);

int ps_nlocations(void);
int ps_nthreads(void);
int ps_current_location(void);

ps_policy_t ps_env_policy(void);
const char *ps_sched_name(void);
const char *ps_dist_name(void);

#endif
