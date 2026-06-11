#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hash.h"

typedef struct {
    uint8_t p;              // precision (4-16 typically)
    uint32_t m;             // number of registers = 1 << p
    uint64_t seed;          // hash seed
    uint8_t* registers;     // dynamic array
} HyperLogLog;

static double hll_alpha(uint8_t p, uint32_t m) {
    switch (p) {
        case 4:
            return 0.673;
        case 5:
            return 0.697;
        case 6:
            return 0.709;
        default:
            return 0.7213 / (1.0 + 1.079 / (double)m);
    }
}

static double* inv_pow2_table = NULL;

static void hll_init_inv_pow2(void) {
    if (inv_pow2_table != NULL) {
        return;
    }
    inv_pow2_table = (double*)malloc(64 * sizeof(double));
    for (int i = 0; i < 64; i++) {
        inv_pow2_table[i] = 1.0 / (double)(1ULL << i);
    }
}

void hll_free_inv_pow2(void) {
    if (inv_pow2_table) {
        free(inv_pow2_table);
        inv_pow2_table = NULL;
    }
}

HyperLogLog* hll_create(uint8_t p, uint64_t seed) {
    if (p < 4 || p > 18) {
        fprintf(stderr, "Error: p must be between 4 and 18\n");
        return NULL;
    }

    HyperLogLog* hll = (HyperLogLog*)malloc(sizeof(HyperLogLog));
    if (!hll) return NULL;

    hll->p = p;
    hll->m = 1 << p;
    hll->seed = seed;
    hll->registers = (uint8_t*)calloc(hll->m, sizeof(uint8_t));

    if (!hll->registers) {
        free(hll);
        return NULL;
    }

    hll_init_inv_pow2();
    return hll;
}

void hll_destroy(HyperLogLog* hll) {
    if (hll) {
        if (hll->registers) free(hll->registers);
        free(hll);
    }
}

void hll_reset(HyperLogLog* hll) {
    if (hll && hll->registers) {
        memset(hll->registers, 0, hll->m);
    }
}

static inline uint8_t hll_rank(uint64_t hash, uint8_t p) {
    uint64_t rest = hash << p;
    if (rest == 0) {
        return 64 - p + 1;
    }
    return __builtin_clzll(rest) + 1;
}

void hll_add(HyperLogLog* hll, const char* element) {
    uint64_t hash = murmur3_64_string(element, hll->seed);
    uint32_t index = hash >> (64 - hll->p);
    uint8_t rank = hll_rank(hash, hll->p);
    if (rank > hll->registers[index]) {
        hll->registers[index] = rank;
    }
}

uint64_t hll_estimate(const HyperLogLog* hll) {
    if (!hll->registers) return 0;

    double sum = 0.0;
    int zero_registers = 0;

    for (uint32_t i = 0; i < hll->m; i++) {
        sum += inv_pow2_table[hll->registers[i]];
        if (hll->registers[i] == 0) {
            zero_registers++;
        }
    }

    double raw = hll_alpha(hll->p, hll->m) * (double)((uint64_t)hll->m * hll->m) / sum;

    if (raw <= 2.5 * hll->m) {
        if (zero_registers > 0) {
            return (uint64_t)((double)hll->m * log((double)hll->m / (double)zero_registers));
        }
        return (uint64_t)raw;
    }

    if (raw < (1ULL << 32) / 30.0) {
        return (uint64_t)raw;
    }

    return (uint64_t)(-(double)(1ULL << 32) * log(1.0 - raw / (double)(1ULL << 32)));
}

void hll_merge(HyperLogLog* dest, const HyperLogLog* src) {
    if (dest->p != src->p || dest->m != src->m) {
        fprintf(stderr, "Error: Cannot merge HLLs with different precision\n");
        return;
    }

    for (uint32_t i = 0; i < dest->m; i++) {
        if (src->registers[i] > dest->registers[i]) {
            dest->registers[i] = src->registers[i];
        }
    }
}

double hll_memory_bytes(const HyperLogLog* hll) {
    return (double)(sizeof(HyperLogLog) + hll->m * sizeof(uint8_t));
}

static char* gen_random_string(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char* str = (char*)malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[length] = '\0';
    return str;
}

void run_test(uint8_t p, uint64_t seed, int unique_count, int test_id, FILE* output) {
    HyperLogLog* hll = hll_create(p, seed);
    if (!hll) {
        fprintf(stderr, "Failed to create HLL with p=%d\n", p);
        return;
    }

    char** elements = (char**)malloc((size_t)unique_count * sizeof(char*));
    for (int i = 0; i < unique_count; i++) {
        elements[i] = gen_random_string(16);
        hll_add(hll, elements[i]);
    }

    uint64_t estimate = hll_estimate(hll);
    double error = fabs((double)((int64_t)estimate - unique_count)) / (double)unique_count * 100.0;
    double memory = hll_memory_bytes(hll);

    fprintf(output, "%d,%d,%d,%llu,%.2f,%.2f,%llu\n",
            test_id, p, unique_count,
            (unsigned long long)estimate, error, memory,
            (unsigned long long)seed);

    HyperLogLog* hll2 = hll_create(p, seed);
    HyperLogLog* hll3 = hll_create(p, seed);

    for (int i = 0; i < unique_count / 2; i++) {
        hll_add(hll2, elements[i]);
    }
    for (int i = unique_count / 2; i < unique_count; i++) {
        hll_add(hll3, elements[i]);
    }

    hll_merge(hll2, hll3);
    uint64_t merge_est = hll_estimate(hll2);
    double merge_error = fabs((double)((int64_t)merge_est - unique_count)) / (double)unique_count * 100.0;

    fprintf(output, "%d,%d,%d,%llu,%.2f,%.2f,%llu,merge,%llu,%.2f\n",
            test_id, p, unique_count,
            (unsigned long long)estimate, error, memory,
            (unsigned long long)seed,
            (unsigned long long)merge_est, merge_error);

    for (int i = 0; i < unique_count; i++) {
        free(elements[i]);
    }
    free(elements);
    hll_destroy(hll);
    hll_destroy(hll2);
    hll_destroy(hll3);
}

int main(int argc, char* argv[]) {
    int unique_count = 1000000;
    uint64_t seed = 0xadc83b19adc83b19ULL;
    uint8_t p_start = 10;
    uint8_t p_end = 16;
    const char* output_file = "results.csv";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            unique_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) {
            p_start = atoi(argv[++i]);
            if (i+1 < argc && argv[i+1][0] != '-') {
                p_end = atoi(argv[++i]);
            } else {
                p_end = p_start;
            }
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            output_file = argv[++i];
        }
    }

    srand(42);

    FILE* output = fopen(output_file, "w");
    if (!output) {
        fprintf(stderr, "Cannot open output file: %s\n", output_file);
        return 1;
    }

    fprintf(output, "test_id,p,unique_count,estimate,error_percent,memory_bytes,seed,merge_type,merge_estimate,merge_error_percent\n");

    int test_id = 0;

    for (uint8_t p = p_start; p <= p_end; p++) {
        printf("Running test with p=%d, unique_count=%d...\n", p, unique_count);
        run_test(p, seed, unique_count, test_id++, output);

        if (unique_count == 1000000) {
            printf("  Testing with smaller cardinalities...\n");
            run_test(p, seed, 10000, test_id++, output);
            run_test(p, seed, 100000, test_id++, output);
            run_test(p, seed, 500000, test_id++, output);
        }
    }

    fclose(output);

    printf("\nResults saved to: %s\n", output_file);
    printf("To generate graphs, use the Python script.\n");

    hll_free_inv_pow2();
    return 0;
}
