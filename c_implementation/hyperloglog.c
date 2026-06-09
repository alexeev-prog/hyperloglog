#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

#define HLL_P 14
#define HLL_M (1 << HLL_P)
#define HLL_SEED 0xadc83b19adc83b19ULL

typedef struct {
    uint8_t registers[HLL_M];
} HyperLogLog;

static double hll_alpha(void) {
    switch (HLL_P) {
        case 4:
            return 0.673;
        case 5:
            return 0.697;
        case 6:
            return 0.709;
        default:
            return 0.7213 / (1.0 + 1.079 / (double)HLL_M);
    }
}

static double inv_pow2_table[64];

static void hll_init_inv_pow2(void) {
    static int initialized = 0;
    if (initialized) {
        return;
    }
    for (int i = 0; i < 64; i++) {
        inv_pow2_table[i] = 1.0 / (double)(1ULL << i);
    }
    initialized = 1;
}

void hll_init(HyperLogLog* hll) {
    memset(hll->registers, 0, HLL_M);
    hll_init_inv_pow2();
}

static inline uint8_t hll_rank(uint64_t hash, uint8_t p) {
    uint64_t rest = hash << p;
    if (rest == 0) {
        return 64 - p + 1;
    }
    return __builtin_clzll(rest) + 1;
}

void hll_add(HyperLogLog* hll, const char* element) {
    uint64_t hash = murmur3_64_string(element, HLL_SEED);
    uint32_t index = hash >> (64 - HLL_P);
    uint8_t rank = hll_rank(hash, HLL_P);
    if (rank > hll->registers[index]) {
        hll->registers[index] = rank;
    }
}

uint64_t hll_estimate(const HyperLogLog* hll) {
    double sum = 0.0;
    int zero_registers = 0;
    for (int i = 0; i < HLL_M; i++) {
        sum += inv_pow2_table[hll->registers[i]];
        if (hll->registers[i] == 0) {
            zero_registers++;
        }
    }
    double raw = hll_alpha() * (double)((uint64_t)HLL_M * HLL_M) / sum;
    if (raw <= 2.5 * HLL_M) {
        if (zero_registers > 0) {
            return (uint64_t)((double)HLL_M * log((double)HLL_M / (double)zero_registers));
        }
        return (uint64_t)raw;
    }
    if (raw < (1ULL << 32) / 30.0) {
        return (uint64_t)raw;
    }
    return (uint64_t)(-(double)(1ULL << 32) * log(1.0 - raw / (double)(1ULL << 32)));
}

void hll_merge(HyperLogLog* dest, const HyperLogLog* src) {
    for (int i = 0; i < HLL_M; i++) {
        if (src->registers[i] > dest->registers[i]) {
            dest->registers[i] = src->registers[i];
        }
    }
}

static char* gen_random_string(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* str = malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[length] = '\0';
    return str;
}

int main(void) {
    srand(42);
    HyperLogLog hll;
    hll_init(&hll);

    const int unique_count = 100000;
    char** elements = malloc((size_t)unique_count * sizeof(char*));
    for (int i = 0; i < unique_count; i++) {
        elements[i] = gen_random_string(16);
        hll_add(&hll, elements[i]);
    }

    uint64_t estimate = hll_estimate(&hll);
    double error = fabs((double)((int64_t)estimate - unique_count)) / (double)unique_count * 100.0;

    printf("Precision (p): %d\n", HLL_P);
    printf("Registers (m): %d\n", HLL_M);
    printf("Memory: %zu bytes\n", sizeof(HyperLogLog));
    printf("Actual unique:  %d\n", unique_count);
    printf("HLL estimate:   %lu\n", (unsigned long)estimate);
    printf("Error:          %.2f%%\n", error);

    uint64_t est2 = hll_estimate(&hll);
    printf("\nIdempotency check: %lu\n", (unsigned long)est2);

    HyperLogLog hll2;
    hll_init(&hll2);
    for (int i = 0; i < unique_count / 2; i++) {
        hll_add(&hll2, elements[i]);
    }

    HyperLogLog hll3;
    hll_init(&hll3);
    for (int i = unique_count / 2; i < unique_count; i++) {
        hll_add(&hll3, elements[i]);
    }

    hll_merge(&hll2, &hll3);
    uint64_t merge_est = hll_estimate(&hll2);
    printf("Merge estimate: %lu\n", (unsigned long)merge_est);

    for (int i = 0; i < unique_count; i++) {
        free(elements[i]);
    }
    free(elements);
    return 0;
}
