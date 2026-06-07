/*
 * Independent D11 re-enumerator for OEIS A096242.
 *
 * This is intentionally separate from src/a096242.c.  It reads D10.bin,
 * generates all valid 11-digit base-9 insertion candidates, globally
 * deduplicates them with qsort/unique, filters by small primes, checks the
 * remaining candidates with GMP, and writes the resulting D11 level.
 *
 * Intended use:
 *   cc -O3 -march=native -std=c17 -Wall -Wextra -Wpedantic -Wshadow \
 *      -Wconversion -Wformat=2 cert/a096242_d11_gmp_reenumerate.c \
 *      -lgmp -o /tmp/a096242_d11_gmp_reenumerate
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gmp.h>

#define BASE 9ULL
#define D10_COUNT 1140171ULL
#define D11_EXPECTED_COUNT 5056361ULL
#define INITIAL_CAND_CAP 112876929ULL

typedef struct {
    uint64_t *data;
    size_t len;
    size_t cap;
} U64Vec;

static const uint32_t small_primes[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
    43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97
};

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *msg) {
    fprintf(stderr, "fatal: %s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
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
        if (next < v->cap) {
            die("vector capacity overflow");
        }
        vec_reserve(v, next);
    }
    v->data[v->len++] = x;
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

static size_t sort_unique(U64Vec *v) {
    if (v->len == 0) {
        return 0;
    }
    qsort(v->data, v->len, sizeof(uint64_t), cmp_u64);
    size_t w = 1;
    for (size_t i = 1; i < v->len; i++) {
        if (v->data[i] != v->data[w - 1]) {
            v->data[w++] = v->data[i];
        }
    }
    v->len = w;
    return w;
}

static bool valid_last_digit(uint64_t x) {
    return (x % BASE) % 3ULL != 0ULL;
}

static bool passes_small_primes(uint64_t x) {
    for (size_t i = 0; i < sizeof(small_primes) / sizeof(small_primes[0]); i++) {
        uint32_t p = small_primes[i];
        if (x == (uint64_t)p) {
            return true;
        }
        if (x % (uint64_t)p == 0ULL) {
            return false;
        }
    }
    return true;
}

static bool gmp_is_prime_u64(mpz_t z, uint64_t x) {
    mpz_set_ui(z, x);
    return mpz_probab_prime_p(z, 25) != 0;
}

static uint64_t *read_u64_file(const char *path, size_t *count_out) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        die_errno("fopen input");
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        die_errno("fseek");
    }
    long sz = ftell(fp);
    if (sz < 0) {
        die_errno("ftell");
    }
    if ((unsigned long)sz % sizeof(uint64_t) != 0UL) {
        die("input size is not a multiple of uint64_t");
    }
    rewind(fp);
    size_t count = (size_t)((unsigned long)sz / sizeof(uint64_t));
    uint64_t *data = (uint64_t *)malloc(count * sizeof(uint64_t));
    if (data == NULL) {
        die_errno("malloc input");
    }
    if (fread(data, sizeof(uint64_t), count, fp) != count) {
        die_errno("fread input");
    }
    if (fclose(fp) != 0) {
        die_errno("fclose input");
    }
    *count_out = count;
    return data;
}

static void write_u64_file(const char *path, const U64Vec *v) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        die_errno("fopen output");
    }
    if (fwrite(v->data, sizeof(uint64_t), v->len, fp) != v->len) {
        die_errno("fwrite output");
    }
    if (fclose(fp) != 0) {
        die_errno("fclose output");
    }
}

static void generate_d11_candidates(const uint64_t *parents, size_t parent_count,
                                    U64Vec *candidates,
                                    uint64_t *insertion_trials,
                                    uint64_t *leading_zero_rejects,
                                    uint64_t *last_digit_rejects) {
    const int output_level = 11;
    vec_reserve(candidates, (size_t)INITIAL_CAND_CAP);

    for (size_t i = 0; i < parent_count; i++) {
        uint64_t parent = parents[i];
        for (int pos = 0; pos < output_level; pos++) {
            uint64_t place = 1;
            for (int k = 0; k < pos; k++) {
                place *= BASE;
            }
            uint64_t low = parent % place;
            uint64_t high = parent / place;
            for (uint64_t digit = 0; digit < BASE; digit++) {
                (*insertion_trials)++;
                if (pos == output_level - 1 && digit == 0) {
                    (*leading_zero_rejects)++;
                    continue;
                }
                uint64_t candidate = high * (place * BASE) + digit * place + low;
                if (!valid_last_digit(candidate)) {
                    (*last_digit_rejects)++;
                    continue;
                }
                vec_push(candidates, candidate);
            }
        }
    }
}

int main(int argc, char **argv) {
    const char *d10_path = argc > 1 ? argv[1] : "levels/D10.bin";
    const char *out_path = argc > 2 ? argv[2] : "/tmp/D11_gmp.bin";

    double t0 = now_seconds();
    size_t parent_count = 0;
    uint64_t *parents = read_u64_file(d10_path, &parent_count);
    if (parent_count != (size_t)D10_COUNT) {
        fprintf(stderr, "fatal: expected %" PRIu64 " D10 parents, got %zu\n",
                (uint64_t)D10_COUNT, parent_count);
        free(parents);
        return EXIT_FAILURE;
    }
    double t_read = now_seconds();

    U64Vec candidates;
    vec_init(&candidates);
    uint64_t insertion_trials = 0;
    uint64_t leading_zero_rejects = 0;
    uint64_t last_digit_rejects = 0;
    generate_d11_candidates(parents, parent_count, &candidates,
                            &insertion_trials, &leading_zero_rejects,
                            &last_digit_rejects);
    free(parents);
    double t_gen = now_seconds();

    uint64_t syntax_kept = (uint64_t)candidates.len;
    sort_unique(&candidates);
    uint64_t unique_candidates = (uint64_t)candidates.len;
    double t_sort = now_seconds();

    U64Vec accepted;
    vec_init(&accepted);
    vec_reserve(&accepted, (size_t)D11_EXPECTED_COUNT + 1024U);

    uint64_t small_prime_rejects = 0;
    uint64_t gmp_calls = 0;
    mpz_t z;
    mpz_init(z);
    for (size_t i = 0; i < candidates.len; i++) {
        uint64_t x = candidates.data[i];
        if (!passes_small_primes(x)) {
            small_prime_rejects++;
            continue;
        }
        gmp_calls++;
        if (gmp_is_prime_u64(z, x)) {
            vec_push(&accepted, x);
        }
    }
    mpz_clear(z);
    double t_prime = now_seconds();
    vec_free(&candidates);

    if (accepted.len != (size_t)D11_EXPECTED_COUNT) {
        fprintf(stderr, "warning: expected %" PRIu64 " D11 primes, got %zu\n",
                (uint64_t)D11_EXPECTED_COUNT, accepted.len);
    }

    write_u64_file(out_path, &accepted);
    double t_write = now_seconds();

    printf("D11 GMP re-enumeration\n");
    printf("input=%s output=%s\n", d10_path, out_path);
    printf("gmp_version=%s\n", gmp_version);
    printf("parents=%zu\n", parent_count);
    printf("insertion_trials=%" PRIu64 "\n", insertion_trials);
    printf("leading_zero_rejects=%" PRIu64 "\n", leading_zero_rejects);
    printf("last_digit_rejects=%" PRIu64 "\n", last_digit_rejects);
    printf("syntax_kept=%" PRIu64 "\n", syntax_kept);
    printf("global_unique=%" PRIu64 "\n", unique_candidates);
    printf("small_prime_rejects=%" PRIu64 "\n", small_prime_rejects);
    printf("gmp_calls=%" PRIu64 "\n", gmp_calls);
    printf("accepted=%zu expected=%" PRIu64 " %s\n",
           accepted.len, (uint64_t)D11_EXPECTED_COUNT,
           accepted.len == (size_t)D11_EXPECTED_COUNT ? "OK" : "MISMATCH");
    printf("timing_read=%.3fs\n", t_read - t0);
    printf("timing_generate=%.3fs\n", t_gen - t_read);
    printf("timing_sort_unique=%.3fs\n", t_sort - t_gen);
    printf("timing_gmp_primality=%.3fs\n", t_prime - t_sort);
    printf("timing_write=%.3fs\n", t_write - t_prime);
    printf("timing_total=%.3fs\n", t_write - t0);

    vec_free(&accepted);
    return EXIT_SUCCESS;
}
