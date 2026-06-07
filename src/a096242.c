#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BASE 9ULL
#define DEFAULT_MAX_N 10
#define MAX_SUPPORTED_N 20
#define DEFAULT_MEMORY_GB 8.0
#define MEMORY_MODE_OVERHEAD_FACTOR 3.0L
#define DEFAULT_CHUNK_PARENTS 50000ULL
#define MAX_KNOWN_TERMS 256
#define MAX_PATH_LEN 4096
#define MERGE_BLOCK_VALUES 1048576ULL
#define MAX_OPEN_RUNS 900

typedef struct {
    uint64_t *data;
    size_t len;
    size_t cap;
} U64Vec;

typedef struct {
    uint64_t insertion_trials;
    uint64_t leading_zero_rejects;
    uint64_t last_digit_rejects;
    uint64_t syntax_kept;
    uint64_t chunk_unique_candidates;
    uint64_t global_unique_candidates;
    uint64_t small_prime_rejects;
    uint64_t miller_rabin_calls;
    uint64_t accepted_primes;
} LevelStats;

typedef enum {
    MODE_AUTO = 0,
    MODE_MEMORY = 1,
    MODE_SEGMENTED = 2
} RunMode;

typedef struct {
    int max_n;
    int threads;
    RunMode mode;
    double memory_gb;
    size_t chunk_parents;
    const char *bfile_path;
    const char *tmp_dir;
    const char *save_levels_dir;
    bool validate;
    bool keep_temp;
} Config;

typedef struct {
    const uint64_t *parents;
    size_t begin;
    size_t end;
    int level;
    U64Vec candidates;
    LevelStats stats;
    int error;
} GenerateTask;

typedef struct {
    const uint64_t *candidates;
    size_t begin;
    size_t end;
    U64Vec accepted;
    LevelStats stats;
    int error;
} PrimeTask;

typedef struct {
    FILE *fp;
    char path[MAX_PATH_LEN];
    uint64_t current;
    bool has_current;
} RunReader;

typedef struct {
    RunReader *runs;
    size_t *idx;
    size_t len;
    size_t cap;
} RunHeap;

static const uint64_t small_primes[] = {
    2ULL, 3ULL, 5ULL, 7ULL, 11ULL, 13ULL, 17ULL, 19ULL, 23ULL, 29ULL,
    31ULL, 37ULL, 41ULL, 43ULL, 47ULL, 53ULL, 59ULL, 61ULL, 67ULL, 71ULL,
    73ULL, 79ULL, 83ULL, 89ULL, 97ULL, 0ULL
};

static void die(const char *message) {
    fprintf(stderr, "fatal: %s\n", message);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *message) {
    fprintf(stderr, "fatal: %s: %s\n", message, strerror(errno));
    exit(EXIT_FAILURE);
}

static void die_pthread(const char *message, int err) {
    fprintf(stderr, "fatal: %s: %s\n", message, strerror(err));
    exit(EXIT_FAILURE);
}

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die_errno("clock_gettime");
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
}

static void ensure_directory(const char *path) {
    if (path == NULL || path[0] == '\0') {
        die("empty directory path");
    }

    char tmp[MAX_PATH_LEN];
    int written = snprintf(tmp, sizeof(tmp), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(tmp)) {
        die("directory path too long");
    }

    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = '\0';
    }

    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                die_errno("mkdir");
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        die_errno("mkdir");
    }
}

static void vec_init(U64Vec *v) {
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static void vec_reserve(U64Vec *v, size_t cap) {
    if (cap <= v->cap) {
        return;
    }
    if (cap > SIZE_MAX / sizeof(uint64_t)) {
        die("vector capacity overflow");
    }
    uint64_t *p = (uint64_t *)realloc(v->data, cap * sizeof(uint64_t));
    if (p == NULL) {
        die_errno("realloc");
    }
    v->data = p;
    v->cap = cap;
}

static void vec_push(U64Vec *v, uint64_t x) {
    if (v->len == v->cap) {
        size_t next = v->cap == 0 ? 1024 : v->cap * 2;
        if (next < v->cap || next > SIZE_MAX / sizeof(uint64_t)) {
            die("vector growth overflow");
        }
        vec_reserve(v, next);
    }
    v->data[v->len++] = x;
}

static void vec_append(U64Vec *dst, const U64Vec *src) {
    if (src->len == 0) {
        return;
    }
    if (dst->len > SIZE_MAX - src->len) {
        die("vector append overflow");
    }
    vec_reserve(dst, dst->len + src->len);
    memcpy(dst->data + dst->len, src->data, src->len * sizeof(uint64_t));
    dst->len += src->len;
}

static void vec_free(U64Vec *v) {
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void sort_unique_vec(U64Vec *v) {
    if (v->len == 0) {
        return;
    }
    qsort(v->data, v->len, sizeof(uint64_t), cmp_u64);
    size_t out = 1;
    for (size_t i = 1; i < v->len; i++) {
        if (v->data[i] != v->data[out - 1]) {
            v->data[out++] = v->data[i];
        }
    }
    v->len = out;
}

static void stats_add(LevelStats *dst, const LevelStats *src) {
    dst->insertion_trials += src->insertion_trials;
    dst->leading_zero_rejects += src->leading_zero_rejects;
    dst->last_digit_rejects += src->last_digit_rejects;
    dst->syntax_kept += src->syntax_kept;
    dst->chunk_unique_candidates += src->chunk_unique_candidates;
    dst->global_unique_candidates += src->global_unique_candidates;
    dst->small_prime_rejects += src->small_prime_rejects;
    dst->miller_rabin_calls += src->miller_rabin_calls;
    dst->accepted_primes += src->accepted_primes;
}

static uint64_t pow_u64_checked(uint64_t base, int exp) {
    uint64_t x = 1;
    for (int i = 0; i < exp; i++) {
        if (x > UINT64_MAX / base) {
            die("power overflow");
        }
        x *= base;
    }
    return x;
}

static uint64_t mul_mod_u64(uint64_t a, uint64_t b, uint64_t mod) {
    return (uint64_t)(((__uint128_t)a * (__uint128_t)b) % (__uint128_t)mod);
}

static uint64_t pow_mod_u64(uint64_t a, uint64_t e, uint64_t mod) {
    uint64_t result = 1;
    uint64_t base = a % mod;
    while (e > 0) {
        if ((e & 1ULL) != 0) {
            result = mul_mod_u64(result, base, mod);
        }
        base = mul_mod_u64(base, base, mod);
        e >>= 1;
    }
    return result;
}

static bool miller_rabin_u64(uint64_t n) {
    /*
     * Implemented Miller-Rabin witness set for the uint64_t range:
     * {2, 325, 9375, 28178, 450775, 9780504, 1795265022}.
     * This seven-witness set appears in Jim Sinclair's computational table
     * and is widely used in deterministic 64-bit primality implementations.
     */
    static const uint64_t witnesses[] = {
        2ULL, 325ULL, 9375ULL, 28178ULL, 450775ULL, 9780504ULL,
        1795265022ULL, 0ULL
    };

    uint64_t d = n - 1;
    unsigned s = 0;
    while ((d & 1ULL) == 0) {
        d >>= 1;
        s++;
    }

    for (size_t i = 0; witnesses[i] != 0; i++) {
        uint64_t a = witnesses[i];
        if (a >= n) {
            continue;
        }
        uint64_t x = pow_mod_u64(a, d, n);
        if (x == 1 || x == n - 1) {
            continue;
        }
        bool witness_passed = false;
        for (unsigned r = 1; r < s; r++) {
            x = mul_mod_u64(x, x, n);
            if (x == n - 1) {
                witness_passed = true;
                break;
            }
        }
        if (!witness_passed) {
            return false;
        }
    }
    return true;
}

static bool small_prime_screen(uint64_t n, uint64_t *reject_counter) {
    for (size_t i = 0; small_primes[i] != 0; i++) {
        uint64_t p = small_primes[i];
        if (n == p) {
            return true;
        }
        if (n % p == 0) {
            (*reject_counter)++;
            return false;
        }
    }
    return true;
}

static bool is_prime_counted(uint64_t n, LevelStats *stats) {
    if (n < 2) {
        return false;
    }
    if (!small_prime_screen(n, &stats->small_prime_rejects)) {
        return false;
    }
    stats->miller_rabin_calls++;
    return miller_rabin_u64(n);
}

static bool valid_last_digit(uint64_t candidate) {
    uint64_t d = candidate % BASE;
    return d % 3ULL != 0ULL;
}

static void generate_range(const uint64_t *parents, size_t begin, size_t end,
                           int level, U64Vec *out, LevelStats *stats) {
    uint64_t pow9[32];
    if (level >= (int)(sizeof(pow9) / sizeof(pow9[0]))) {
        die("level exceeds internal pow9 table");
    }
    for (int i = 0; i <= level; i++) {
        pow9[i] = pow_u64_checked(BASE, i);
    }

    for (size_t i = begin; i < end; i++) {
        uint64_t parent = parents[i];
        for (int pos = 0; pos < level; pos++) {
            uint64_t place = pow9[pos];
            uint64_t low = parent % place;
            uint64_t high = parent / place;
            for (uint64_t digit = 0; digit < BASE; digit++) {
                stats->insertion_trials++;
                if (pos == level - 1 && digit == 0) {
                    stats->leading_zero_rejects++;
                    continue;
                }
                uint64_t candidate = high * (place * BASE) + digit * place + low;
                if (!valid_last_digit(candidate)) {
                    stats->last_digit_rejects++;
                    continue;
                }
                stats->syntax_kept++;
                vec_push(out, candidate);
            }
        }
    }
}

static void *generate_worker(void *arg) {
    GenerateTask *task = (GenerateTask *)arg;
    vec_init(&task->candidates);
    __uint128_t bound = (__uint128_t)(uint64_t)(task->end - task->begin) *
                        (__uint128_t)(uint64_t)task->level * (__uint128_t)BASE;
    if (bound > SIZE_MAX) {
        die("worker candidate capacity overflow");
    }
    if (bound > 0) {
        vec_reserve(&task->candidates, (size_t)bound);
    }
    task->error = 0;
    generate_range(task->parents, task->begin, task->end, task->level,
                   &task->candidates, &task->stats);
    return NULL;
}

static void *prime_worker(void *arg) {
    PrimeTask *task = (PrimeTask *)arg;
    vec_init(&task->accepted);
    task->error = 0;
    for (size_t i = task->begin; i < task->end; i++) {
        uint64_t x = task->candidates[i];
        if (is_prime_counted(x, &task->stats)) {
            vec_push(&task->accepted, x);
        }
    }
    return NULL;
}

static int bounded_threads(int requested, size_t work_items) {
    if (requested < 1) {
        requested = 1;
    }
    if (work_items == 0) {
        return 1;
    }
    if ((size_t)requested > work_items) {
        return (int)work_items;
    }
    return requested;
}

static void run_generation_parallel(const uint64_t *parents, size_t parent_count,
                                    int level, int threads, U64Vec *out,
                                    LevelStats *stats) {
    int tcount = bounded_threads(threads, parent_count);
    pthread_t *tids = (pthread_t *)calloc((size_t)tcount, sizeof(*tids));
    GenerateTask *tasks = (GenerateTask *)calloc((size_t)tcount, sizeof(*tasks));
    if (tids == NULL || tasks == NULL) {
        die_errno("calloc");
    }

    for (int t = 0; t < tcount; t++) {
        size_t begin = ((size_t)t * parent_count) / (size_t)tcount;
        size_t end = ((size_t)(t + 1) * parent_count) / (size_t)tcount;
        tasks[t].parents = parents;
        tasks[t].begin = begin;
        tasks[t].end = end;
        tasks[t].level = level;
        int err = pthread_create(&tids[t], NULL, generate_worker, &tasks[t]);
        if (err != 0) {
            for (int j = 0; j < t; j++) {
                int join_err = pthread_join(tids[j], NULL);
                if (join_err != 0) {
                    die_pthread("pthread_join after create failure", join_err);
                }
                vec_free(&tasks[j].candidates);
            }
            free(tids);
            free(tasks);
            die_pthread("pthread_create", err);
        }
    }

    vec_init(out);
    for (int t = 0; t < tcount; t++) {
        int err = pthread_join(tids[t], NULL);
        if (err != 0) {
            die_pthread("pthread_join", err);
        }
        if (tasks[t].error != 0) {
            die("generation worker failed");
        }
        stats_add(stats, &tasks[t].stats);
        vec_append(out, &tasks[t].candidates);
        vec_free(&tasks[t].candidates);
    }

    free(tids);
    free(tasks);
}

static void filter_primes_parallel(const uint64_t *candidates, size_t candidate_count,
                                   int threads, U64Vec *accepted,
                                   LevelStats *stats) {
    int tcount = bounded_threads(threads, candidate_count);
    pthread_t *tids = (pthread_t *)calloc((size_t)tcount, sizeof(*tids));
    PrimeTask *tasks = (PrimeTask *)calloc((size_t)tcount, sizeof(*tasks));
    if (tids == NULL || tasks == NULL) {
        die_errno("calloc");
    }

    for (int t = 0; t < tcount; t++) {
        size_t begin = ((size_t)t * candidate_count) / (size_t)tcount;
        size_t end = ((size_t)(t + 1) * candidate_count) / (size_t)tcount;
        tasks[t].candidates = candidates;
        tasks[t].begin = begin;
        tasks[t].end = end;
        int err = pthread_create(&tids[t], NULL, prime_worker, &tasks[t]);
        if (err != 0) {
            for (int j = 0; j < t; j++) {
                int join_err = pthread_join(tids[j], NULL);
                if (join_err != 0) {
                    die_pthread("pthread_join after create failure", join_err);
                }
                vec_free(&tasks[j].accepted);
            }
            free(tids);
            free(tasks);
            die_pthread("pthread_create", err);
        }
    }

    vec_init(accepted);
    for (int t = 0; t < tcount; t++) {
        int err = pthread_join(tids[t], NULL);
        if (err != 0) {
            die_pthread("pthread_join", err);
        }
        if (tasks[t].error != 0) {
            die("prime worker failed");
        }
        stats_add(stats, &tasks[t].stats);
        vec_append(accepted, &tasks[t].accepted);
        vec_free(&tasks[t].accepted);
    }

    free(tids);
    free(tasks);
}

static void compute_next_memory(const U64Vec *parents, int level, const Config *cfg,
                                U64Vec *next, LevelStats *stats) {
    U64Vec candidates;
    double t0 = now_seconds();
    run_generation_parallel(parents->data, parents->len, level, cfg->threads,
                            &candidates, stats);
    double t1 = now_seconds();
    sort_unique_vec(&candidates);
    stats->global_unique_candidates = (uint64_t)candidates.len;
    stats->chunk_unique_candidates = (uint64_t)candidates.len;
    double t2 = now_seconds();
    filter_primes_parallel(candidates.data, candidates.len, cfg->threads, next, stats);
    double t3 = now_seconds();
    stats->accepted_primes = (uint64_t)next->len;

    fprintf(stderr,
            "level %d timing: generate %.3fs, sort_unique %.3fs, primality %.3fs\n",
            level, t1 - t0, t2 - t1, t3 - t2);
    vec_free(&candidates);
}

static void heap_init(RunHeap *heap, RunReader *runs, size_t cap) {
    heap->runs = runs;
    heap->idx = (size_t *)malloc(cap * sizeof(size_t));
    if (heap->idx == NULL) {
        die_errno("malloc");
    }
    heap->len = 0;
    heap->cap = cap;
}

static bool heap_less(const RunHeap *heap, size_t a, size_t b) {
    return heap->runs[a].current < heap->runs[b].current;
}

static void heap_push(RunHeap *heap, size_t run_idx) {
    if (heap->len == heap->cap) {
        die("run heap overflow");
    }
    size_t i = heap->len++;
    heap->idx[i] = run_idx;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        if (!heap_less(heap, heap->idx[i], heap->idx[p])) {
            break;
        }
        size_t tmp = heap->idx[i];
        heap->idx[i] = heap->idx[p];
        heap->idx[p] = tmp;
        i = p;
    }
}

static size_t heap_pop(RunHeap *heap) {
    size_t out = heap->idx[0];
    heap->idx[0] = heap->idx[--heap->len];
    size_t i = 0;
    for (;;) {
        size_t l = 2 * i + 1;
        size_t r = l + 1;
        size_t m = i;
        if (l < heap->len && heap_less(heap, heap->idx[l], heap->idx[m])) {
            m = l;
        }
        if (r < heap->len && heap_less(heap, heap->idx[r], heap->idx[m])) {
            m = r;
        }
        if (m == i) {
            break;
        }
        size_t tmp = heap->idx[i];
        heap->idx[i] = heap->idx[m];
        heap->idx[m] = tmp;
        i = m;
    }
    return out;
}

static void heap_free(RunHeap *heap) {
    free(heap->idx);
    heap->idx = NULL;
    heap->len = 0;
    heap->cap = 0;
}

static bool read_next_run_value(RunReader *run) {
    uint64_t x;
    size_t n = fread(&x, sizeof(x), 1, run->fp);
    if (n == 1) {
        run->current = x;
        run->has_current = true;
        return true;
    }
    if (ferror(run->fp)) {
        die_errno("fread");
    }
    run->has_current = false;
    return false;
}

static void write_run_file(const char *path, const U64Vec *values) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        die_errno("fopen run for write");
    }
    if (values->len > 0 &&
        fwrite(values->data, sizeof(uint64_t), values->len, fp) != values->len) {
        die_errno("fwrite run");
    }
    if (fclose(fp) != 0) {
        die_errno("fclose run");
    }
}

static void process_merge_block(U64Vec *block, U64Vec *next,
                                const Config *cfg, LevelStats *stats) {
    if (block->len == 0) {
        return;
    }
    U64Vec accepted;
    filter_primes_parallel(block->data, block->len, cfg->threads, &accepted, stats);
    vec_append(next, &accepted);
    vec_free(&accepted);
    block->len = 0;
}

static void merge_runs_and_filter(RunReader *runs, size_t run_count,
                                  const Config *cfg, U64Vec *next,
                                  LevelStats *stats) {
    RunHeap heap;
    heap_init(&heap, runs, run_count);
    for (size_t i = 0; i < run_count; i++) {
        runs[i].fp = fopen(runs[i].path, "rb");
        if (runs[i].fp == NULL) {
            die_errno("fopen run for read");
        }
        if (read_next_run_value(&runs[i])) {
            heap_push(&heap, i);
        }
    }

    U64Vec block;
    vec_init(&block);
    vec_reserve(&block, (size_t)MERGE_BLOCK_VALUES);
    vec_init(next);

    uint64_t last = 0;
    bool have_last = false;
    while (heap.len > 0) {
        size_t idx = heap_pop(&heap);
        uint64_t x = runs[idx].current;
        if (!have_last || x != last) {
            vec_push(&block, x);
            stats->global_unique_candidates++;
            last = x;
            have_last = true;
            if (block.len >= (size_t)MERGE_BLOCK_VALUES) {
                process_merge_block(&block, next, cfg, stats);
            }
        }
        if (read_next_run_value(&runs[idx])) {
            heap_push(&heap, idx);
        }
    }
    process_merge_block(&block, next, cfg, stats);
    stats->accepted_primes = (uint64_t)next->len;
    vec_free(&block);

    for (size_t i = 0; i < run_count; i++) {
        if (fclose(runs[i].fp) != 0) {
            die_errno("fclose run read");
        }
        runs[i].fp = NULL;
        if (!cfg->keep_temp && unlink(runs[i].path) != 0) {
            die_errno("unlink run");
        }
    }
    heap_free(&heap);
}

static void compute_next_segmented(const U64Vec *parents, int level, const Config *cfg,
                                   U64Vec *next, LevelStats *stats) {
    if (parents->len == 0) {
        vec_init(next);
        return;
    }

    size_t chunk = cfg->chunk_parents;
    if (chunk == 0) {
        die("chunk parent count must be positive");
    }
    size_t run_count = parents->len / chunk + (parents->len % chunk != 0 ? 1ULL : 0ULL);
    if (run_count > MAX_OPEN_RUNS) {
        fprintf(stderr,
                "fatal: segmented mode would create %zu run files, above limit %d; increase --chunk-parents\n",
                run_count, MAX_OPEN_RUNS);
        exit(EXIT_FAILURE);
    }
    RunReader *runs = (RunReader *)calloc(run_count, sizeof(*runs));
    if (runs == NULL) {
        die_errno("calloc runs");
    }

    double t0 = now_seconds();
    for (size_t r = 0; r < run_count; r++) {
        size_t begin = r * chunk;
        size_t end = begin + chunk;
        if (end > parents->len) {
            end = parents->len;
        }
        U64Vec candidates;
        LevelStats local_stats = {0};
        run_generation_parallel(parents->data + begin, end - begin, level,
                                cfg->threads, &candidates, &local_stats);
        sort_unique_vec(&candidates);
        local_stats.chunk_unique_candidates = (uint64_t)candidates.len;
        stats_add(stats, &local_stats);

        int written = snprintf(runs[r].path, sizeof(runs[r].path),
                               "%s/A096242_pid%ld_L%02d_run%04zu.bin",
                               cfg->tmp_dir, (long)getpid(), level, r);
        if (written < 0 || (size_t)written >= sizeof(runs[r].path)) {
            die("temporary run path too long");
        }
        write_run_file(runs[r].path, &candidates);
        fprintf(stderr, "level %d segment %zu/%zu: parents=%zu unique=%zu\n",
                level, r + 1, run_count, end - begin, candidates.len);
        vec_free(&candidates);
    }
    double t1 = now_seconds();
    merge_runs_and_filter(runs, run_count, cfg, next, stats);
    double t2 = now_seconds();

    fprintf(stderr,
            "level %d timing: segment_sort_write %.3fs, merge_primality %.3fs\n",
            level, t1 - t0, t2 - t1);
    free(runs);
}

static uint64_t estimate_candidate_bound(size_t parent_count, int level) {
    __uint128_t bound = (__uint128_t)(uint64_t)parent_count *
                        (__uint128_t)(uint64_t)level * (__uint128_t)BASE;
    if (bound > UINT64_MAX) {
        return UINT64_MAX;
    }
    return (uint64_t)bound;
}

static bool should_use_memory_mode(const U64Vec *parents, int level, const Config *cfg) {
    if (cfg->mode == MODE_MEMORY) {
        return true;
    }
    if (cfg->mode == MODE_SEGMENTED) {
        return false;
    }
    uint64_t bound = estimate_candidate_bound(parents->len, level);
    long double bytes = (long double)bound * (long double)sizeof(uint64_t) *
                        MEMORY_MODE_OVERHEAD_FACTOR;
    long double limit = (long double)cfg->memory_gb * 1024.0L * 1024.0L * 1024.0L;
    return bytes <= limit;
}

static void write_level_binary(const char *dir, int level, const U64Vec *values) {
    char path[MAX_PATH_LEN];
    int written = snprintf(path, sizeof(path), "%s/D%02d.bin", dir, level);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        die("level output path too long");
    }
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        die_errno("fopen level output");
    }
    if (values->len > 0 &&
        fwrite(values->data, sizeof(uint64_t), values->len, fp) != values->len) {
        die_errno("fwrite level output");
    }
    if (fclose(fp) != 0) {
        die_errno("fclose level output");
    }
}

static size_t load_bfile_terms(const char *path, uint64_t *terms, size_t cap) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        die_errno("fopen b-file");
    }

    size_t nterms = 0;
    size_t line_no = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_no++;
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            die("b-file line too long");
        }

        char *p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }

        errno = 0;
        char *end = NULL;
        unsigned long long n = strtoull(p, &end, 10);
        if (errno != 0 || end == p) {
            fprintf(stderr, "invalid b-file index at line %zu\n", line_no);
            exit(EXIT_FAILURE);
        }
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }

        errno = 0;
        unsigned long long value = strtoull(p, &end, 10);
        if (errno != 0 || end == p) {
            fprintf(stderr, "invalid b-file value at line %zu\n", line_no);
            exit(EXIT_FAILURE);
        }
        p = end;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '\0' && *p != '#') {
            fprintf(stderr, "trailing garbage in b-file at line %zu\n", line_no);
            exit(EXIT_FAILURE);
        }

        if (n == 0 || n >= cap) {
            die("b-file index outside supported range");
        }
        terms[n] = (uint64_t)value;
        if ((size_t)n > nterms) {
            nterms = (size_t)n;
        }
    }
    if (ferror(fp)) {
        die_errno("fgets b-file");
    }
    if (fclose(fp) != 0) {
        die_errno("fclose b-file");
    }
    return nterms;
}

static void print_stats(int level, const LevelStats *s, const char *mode_name) {
    printf("n=%d mode=%s insertion_trials=%" PRIu64
           " leading_zero_rejects=%" PRIu64
           " last_digit_rejects=%" PRIu64
           " syntax_kept=%" PRIu64
           " chunk_unique=%" PRIu64
           " global_unique=%" PRIu64
           " small_prime_rejects=%" PRIu64
           " mr_calls=%" PRIu64
           " accepted=%" PRIu64 "\n",
           level, mode_name,
           s->insertion_trials,
           s->leading_zero_rejects,
           s->last_digit_rejects,
           s->syntax_kept,
           s->chunk_unique_candidates,
           s->global_unique_candidates,
           s->small_prime_rejects,
           s->miller_rabin_calls,
           s->accepted_primes);
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  --max-n N              compute through level N (default %d)\n"
            "  --threads N            worker threads (default: online CPU count)\n"
            "  --mode auto|memory|segmented\n"
            "  --memory-gb X          auto-mode in-memory candidate limit (default %.1f)\n"
            "  --chunk-parents N      parent chunk size for segmented mode (default %" PRIu64 ")\n"
            "  --tmp-dir DIR          temporary run directory (default tmp)\n"
            "  --save-levels DIR      write sorted D_n binary files to DIR\n"
            "  --bfile PATH           known b-file for validation (default data/b096242.txt)\n"
            "  --validate             compare available known terms while computing\n"
            "  --keep-temp            keep segmented temporary run files\n"
            "  --help                 show this help\n"
            "\n"
            "Notes:\n"
            "  uint64_t mode supports --max-n up to %d for base-9 values.\n"
            "  If an option is specified multiple times, the last value is used.\n",
            argv0, DEFAULT_MAX_N, DEFAULT_MEMORY_GB, (uint64_t)DEFAULT_CHUNK_PARENTS,
            MAX_SUPPORTED_N);
}

static int parse_int_arg(const char *s, const char *name) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 1 || v > INT_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, s);
        exit(EXIT_FAILURE);
    }
    return (int)v;
}

static size_t parse_size_arg(const char *s, const char *name) {
    if (s[0] == '-' || s[0] == '+') {
        fprintf(stderr, "invalid %s: %s\n", name, s);
        exit(EXIT_FAILURE);
    }
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v == 0 || v > SIZE_MAX) {
        fprintf(stderr, "invalid %s: %s\n", name, s);
        exit(EXIT_FAILURE);
    }
    return (size_t)v;
}

static double parse_double_arg(const char *s, const char *name) {
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0' || !isfinite(v) || v <= 0.0) {
        fprintf(stderr, "invalid %s: %s\n", name, s);
        exit(EXIT_FAILURE);
    }
    return v;
}

static const char *require_arg(int argc, char **argv, int *i) {
    if (*i + 1 >= argc) {
        fprintf(stderr, "missing argument for %s\n", argv[*i]);
        exit(EXIT_FAILURE);
    }
    return argv[++(*i)];
}

static void parse_args(int argc, char **argv, Config *cfg) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cfg->max_n = DEFAULT_MAX_N;
    cfg->threads = cpus > 0 && cpus < INT_MAX ? (int)cpus : 1;
    cfg->mode = MODE_AUTO;
    cfg->memory_gb = DEFAULT_MEMORY_GB;
    cfg->chunk_parents = (size_t)DEFAULT_CHUNK_PARENTS;
    cfg->bfile_path = "data/b096242.txt";
    cfg->tmp_dir = "tmp";
    cfg->save_levels_dir = NULL;
    cfg->validate = false;
    cfg->keep_temp = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--max-n") == 0) {
            cfg->max_n = parse_int_arg(require_arg(argc, argv, &i), "max-n");
        } else if (strcmp(argv[i], "--threads") == 0) {
            cfg->threads = parse_int_arg(require_arg(argc, argv, &i), "threads");
        } else if (strcmp(argv[i], "--mode") == 0) {
            const char *m = require_arg(argc, argv, &i);
            if (strcmp(m, "auto") == 0) {
                cfg->mode = MODE_AUTO;
            } else if (strcmp(m, "memory") == 0) {
                cfg->mode = MODE_MEMORY;
            } else if (strcmp(m, "segmented") == 0) {
                cfg->mode = MODE_SEGMENTED;
            } else {
                fprintf(stderr, "invalid mode: %s\n", m);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--memory-gb") == 0) {
            cfg->memory_gb = parse_double_arg(require_arg(argc, argv, &i), "memory-gb");
        } else if (strcmp(argv[i], "--chunk-parents") == 0) {
            cfg->chunk_parents = parse_size_arg(require_arg(argc, argv, &i), "chunk-parents");
        } else if (strcmp(argv[i], "--tmp-dir") == 0) {
            cfg->tmp_dir = require_arg(argc, argv, &i);
        } else if (strcmp(argv[i], "--save-levels") == 0) {
            cfg->save_levels_dir = require_arg(argc, argv, &i);
        } else if (strcmp(argv[i], "--bfile") == 0) {
            cfg->bfile_path = require_arg(argc, argv, &i);
        } else if (strcmp(argv[i], "--validate") == 0) {
            cfg->validate = true;
        } else if (strcmp(argv[i], "--keep-temp") == 0) {
            cfg->keep_temp = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (cfg->max_n > MAX_SUPPORTED_N) {
        fprintf(stderr,
                "fatal: --max-n %d exceeds uint64_t base-9 support; maximum is %d\n",
                cfg->max_n, MAX_SUPPORTED_N);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    Config cfg;
    parse_args(argc, argv, &cfg);
    ensure_directory(cfg.tmp_dir);
    if (cfg.save_levels_dir != NULL) {
        ensure_directory(cfg.save_levels_dir);
    }

    uint64_t known[MAX_KNOWN_TERMS] = {0};
    size_t known_count = 0;
    if (cfg.validate) {
        known_count = load_bfile_terms(cfg.bfile_path, known, MAX_KNOWN_TERMS);
        if (known_count == 0) {
            die("validation requested but b-file contains no terms");
        }
    }

    U64Vec current;
    vec_init(&current);
    vec_push(&current, 2);
    vec_push(&current, 3);
    vec_push(&current, 5);
    vec_push(&current, 7);

    printf("A096242: max_n=%d threads=%d mode=%s memory_gb=%.3f chunk_parents=%zu\n",
           cfg.max_n, cfg.threads,
           cfg.mode == MODE_AUTO ? "auto" : (cfg.mode == MODE_MEMORY ? "memory" : "segmented"),
           cfg.memory_gb, cfg.chunk_parents);
    printf("n=1 accepted=%zu", current.len);
    if (cfg.validate && known_count >= 1) {
        printf(" expected=%" PRIu64 " %s", known[1],
               known[1] == current.len ? "OK" : "MISMATCH");
        if (known[1] != current.len) {
            printf("\n");
            vec_free(&current);
            return EXIT_FAILURE;
        }
    }
    printf("\n");

    if (cfg.save_levels_dir != NULL) {
        write_level_binary(cfg.save_levels_dir, 1, &current);
    }

    for (int level = 2; level <= cfg.max_n; level++) {
        bool memory_mode = should_use_memory_mode(&current, level, &cfg);
        const char *mode_name = memory_mode ? "memory" : "segmented";
        uint64_t bound = estimate_candidate_bound(current.len, level);
        fprintf(stderr,
                "level %d start: parents=%zu candidate_bound=%" PRIu64 " selected_mode=%s\n",
                level, current.len, bound, mode_name);

        U64Vec next;
        LevelStats stats = {0};
        double t0 = now_seconds();
        if (memory_mode) {
            compute_next_memory(&current, level, &cfg, &next, &stats);
        } else {
            compute_next_segmented(&current, level, &cfg, &next, &stats);
        }
        double t1 = now_seconds();
        print_stats(level, &stats, mode_name);
        printf("n=%d accepted=%zu elapsed=%.3fs", level, next.len, t1 - t0);
        if (cfg.validate && (size_t)level <= known_count) {
            printf(" expected=%" PRIu64 " %s", known[level],
                   known[level] == next.len ? "OK" : "MISMATCH");
            if (known[level] != next.len) {
                printf("\n");
                vec_free(&current);
                vec_free(&next);
                return EXIT_FAILURE;
            }
        }
        printf("\n");

        if (cfg.save_levels_dir != NULL) {
            write_level_binary(cfg.save_levels_dir, level, &next);
        }
        vec_free(&current);
        current = next;
    }

    vec_free(&current);
    return EXIT_SUCCESS;
}
