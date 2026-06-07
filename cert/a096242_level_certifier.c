#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define BASE 9ULL
#define MAX_LEVEL 20
#define MAX_PATH 4096

typedef struct {
    uint64_t *v;
    size_t n;
} Level;

typedef struct {
    const Level *cur;
    const Level *prev;
    const uint64_t *pow9;
    int level;
    size_t begin;
    size_t end;
    size_t bad_prime;
    size_t bad_deletion;
} Task;

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *msg) {
    fprintf(stderr, "fatal: %s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die_errno("clock_gettime");
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static uint64_t pow_u64(uint64_t a, int e) {
    uint64_t r = 1;
    for (int i = 0; i < e; i++) {
        if (a != 0 && r > UINT64_MAX / a) {
            die("power overflow");
        }
        r *= a;
    }
    return r;
}

static uint64_t mul_mod(uint64_t a, uint64_t b, uint64_t m) {
    return (uint64_t)(((__uint128_t)a * (__uint128_t)b) % (__uint128_t)m);
}

static uint64_t pow_mod(uint64_t a, uint64_t e, uint64_t m) {
    uint64_t r = 1 % m;
    uint64_t x = a % m;
    while (e != 0) {
        if ((e & 1ULL) != 0) {
            r = mul_mod(r, x, m);
        }
        x = mul_mod(x, x, m);
        e >>= 1U;
    }
    return r;
}

static bool is_prime_u64(uint64_t n) {
    static const uint64_t small[] = {
        2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 0
    };
    static const uint64_t witnesses[] = {
        2, 325, 9375, 28178, 450775, 9780504, 1795265022
    };

    if (n < 2) {
        return false;
    }
    for (size_t i = 0; small[i] != 0; i++) {
        if (n == small[i]) {
            return true;
        }
        if (n % small[i] == 0) {
            return false;
        }
    }

    uint64_t d = n - 1;
    unsigned s = 0;
    while ((d & 1ULL) == 0) {
        d >>= 1U;
        s++;
    }

    for (size_t i = 0; i < sizeof(witnesses) / sizeof(witnesses[0]); i++) {
        uint64_t a = witnesses[i] % n;
        if (a == 0) {
            continue;
        }
        uint64_t x = pow_mod(a, d, n);
        if (x == 1 || x == n - 1) {
            continue;
        }
        bool passed = false;
        for (unsigned r = 1; r < s; r++) {
            x = mul_mod(x, x, n);
            if (x == n - 1) {
                passed = true;
                break;
            }
        }
        if (!passed) {
            return false;
        }
    }
    return true;
}

static bool contains_u64(const Level *level, uint64_t x) {
    size_t lo = 0;
    size_t hi = level->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (level->v[mid] < x) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < level->n && level->v[lo] == x;
}

static bool has_valid_deletion(uint64_t x, int level, const Level *prev,
                               const uint64_t *pow9) {
    for (int pos = 0; pos < level; pos++) {
        uint64_t place = pow9[pos];
        uint64_t low = x % place;
        uint64_t high = x / (place * BASE);
        uint64_t reduced = high * place + low;
        if (contains_u64(prev, reduced)) {
            return true;
        }
    }
    return false;
}

static void *worker(void *arg) {
    Task *task = (Task *)arg;
    for (size_t i = task->begin; i < task->end; i++) {
        uint64_t x = task->cur->v[i];
        if (!is_prime_u64(x)) {
            task->bad_prime++;
            if (task->bad_prime <= 3) {
                fprintf(stderr, "level %d non-prime value at index %zu: %" PRIu64 "\n",
                        task->level, i, x);
            }
        }
        if (!has_valid_deletion(x, task->level, task->prev, task->pow9)) {
            task->bad_deletion++;
            if (task->bad_deletion <= 3) {
                fprintf(stderr, "level %d value without valid deletion at index %zu: %" PRIu64 "\n",
                        task->level, i, x);
            }
        }
    }
    return NULL;
}

static Level load_level(const char *dir, int level) {
    char path[MAX_PATH];
    int written = snprintf(path, sizeof(path), "%s/D%02d.bin", dir, level);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        die("level path too long");
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        die_errno(path);
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        die_errno("stat");
    }
    if (st.st_size < 0 || (st.st_size % (off_t)sizeof(uint64_t)) != 0) {
        die("level file size is not a uint64_t multiple");
    }
    size_t n = (size_t)st.st_size / sizeof(uint64_t);
    uint64_t *v = NULL;
    if (n != 0) {
        v = (uint64_t *)malloc(n * sizeof(*v));
        if (v == NULL) {
            die_errno("malloc level");
        }
        if (fread(v, sizeof(*v), n, fp) != n) {
            die_errno("fread level");
        }
    }
    if (fclose(fp) != 0) {
        die_errno("fclose level");
    }

    Level out = {v, n};
    return out;
}

static void free_level(Level *level) {
    free(level->v);
    level->v = NULL;
    level->n = 0;
}

static bool validate_sorted_range(const Level *level, int digit_level,
                                  const uint64_t *pow9) {
    uint64_t lo = digit_level == 1 ? 0 : pow9[digit_level - 1];
    uint64_t hi = pow9[digit_level];
    bool ok = true;
    for (size_t i = 0; i < level->n; i++) {
        uint64_t x = level->v[i];
        if (x < lo || x >= hi) {
            fprintf(stderr, "level %d range failure at index %zu: %" PRIu64 "\n",
                    digit_level, i, x);
            ok = false;
            break;
        }
        if (i != 0 && x <= level->v[i - 1]) {
            fprintf(stderr, "level %d sort/duplicate failure at index %zu: %" PRIu64 "\n",
                    digit_level, i, x);
            ok = false;
            break;
        }
    }
    return ok;
}

static bool validate_d1(const Level *level) {
    static const uint64_t expected[] = {2, 3, 5, 7};
    if (level->n != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr, "D01 count mismatch: %zu\n", level->n);
        return false;
    }
    for (size_t i = 0; i < level->n; i++) {
        if (level->v[i] != expected[i]) {
            fprintf(stderr, "D01 mismatch at index %zu: %" PRIu64 "\n", i, level->v[i]);
            return false;
        }
    }
    return true;
}

static void certify_level(const Level *cur, const Level *prev, int level,
                          int threads, const uint64_t *pow9) {
    if (threads < 1) {
        threads = 1;
    }
    if ((size_t)threads > cur->n && cur->n != 0) {
        threads = (int)cur->n;
    }
    pthread_t *tids = (pthread_t *)calloc((size_t)threads, sizeof(*tids));
    Task *tasks = (Task *)calloc((size_t)threads, sizeof(*tasks));
    if (tids == NULL || tasks == NULL) {
        die_errno("calloc tasks");
    }

    double t0 = now_seconds();
    for (int t = 0; t < threads; t++) {
        size_t begin = ((size_t)t * cur->n) / (size_t)threads;
        size_t end = ((size_t)(t + 1) * cur->n) / (size_t)threads;
        tasks[t].cur = cur;
        tasks[t].prev = prev;
        tasks[t].pow9 = pow9;
        tasks[t].level = level;
        tasks[t].begin = begin;
        tasks[t].end = end;
        int err = pthread_create(&tids[t], NULL, worker, &tasks[t]);
        if (err != 0) {
            fprintf(stderr, "fatal: pthread_create: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
    }

    size_t bad_prime = 0;
    size_t bad_deletion = 0;
    for (int t = 0; t < threads; t++) {
        int err = pthread_join(tids[t], NULL);
        if (err != 0) {
            fprintf(stderr, "fatal: pthread_join: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
        bad_prime += tasks[t].bad_prime;
        bad_deletion += tasks[t].bad_deletion;
    }
    double t1 = now_seconds();
    free(tids);
    free(tasks);

    printf("D%02d: count=%zu prime_bad=%zu deletion_bad=%zu elapsed=%.3fs %s\n",
           level, cur->n, bad_prime, bad_deletion, t1 - t0,
           (bad_prime == 0 && bad_deletion == 0) ? "OK" : "FAIL");
    if (bad_prime != 0 || bad_deletion != 0) {
        exit(EXIT_FAILURE);
    }
}

static int parse_int(const char *s, const char *name) {
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 1 || v > INT_MAX) {
        fprintf(stderr, "fatal: invalid %s: %s\n", name, s);
        exit(EXIT_FAILURE);
    }
    return (int)v;
}

int main(int argc, char **argv) {
    const char *levels_dir = "levels";
    int max_n = 14;
    int threads = 16;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--levels") == 0) {
            if (++i >= argc) {
                die("missing argument for --levels");
            }
            levels_dir = argv[i];
        } else if (strcmp(argv[i], "--max-n") == 0) {
            if (++i >= argc) {
                die("missing argument for --max-n");
            }
            max_n = parse_int(argv[i], "max-n");
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (++i >= argc) {
                die("missing argument for --threads");
            }
            threads = parse_int(argv[i], "threads");
        } else {
            fprintf(stderr, "usage: %s [--levels DIR] [--max-n N] [--threads N]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (max_n > MAX_LEVEL) {
        die("max-n exceeds certifier limit");
    }

    uint64_t pow9[MAX_LEVEL + 1];
    for (int i = 0; i <= MAX_LEVEL; i++) {
        pow9[i] = pow_u64(BASE, i);
    }

    printf("A096242 level certifier: levels=%s max_n=%d threads=%d\n",
           levels_dir, max_n, threads);
    printf("Scope: verifies saved-level validity (sorted/range, primality, deletion parent).\n");
    printf("It does not perform a second missing-candidate enumeration.\n");

    Level prev = load_level(levels_dir, 1);
    if (!validate_sorted_range(&prev, 1, pow9) || !validate_d1(&prev)) {
        return EXIT_FAILURE;
    }
    printf("D01: count=%zu base_case OK\n", prev.n);

    for (int level = 2; level <= max_n; level++) {
        Level cur = load_level(levels_dir, level);
        if (!validate_sorted_range(&cur, level, pow9)) {
            free_level(&prev);
            free_level(&cur);
            return EXIT_FAILURE;
        }
        certify_level(&cur, &prev, level, threads, pow9);
        free_level(&prev);
        prev = cur;
    }

    free_level(&prev);
    printf("CERTIFIER RESULT: PASS through n=%d\n", max_n);
    return EXIT_SUCCESS;
}
