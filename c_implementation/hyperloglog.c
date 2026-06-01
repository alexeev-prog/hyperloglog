/**
 * @file hyperloglog.c
 * @author Alexeev Bronislav (alexeev.dev@mail.ru)
 * @brief Implementation of HyperLogLog algorithm.
 * @version 0.1
 * @date 2026-05-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "hash.h"
#include "stdio.h"

void print_binary(uint32_t num) {
    for (int i = 31; i >= 0; i--) {
        printf("%d", (num >> i) & 1);
        if (i % 4 == 0) printf(" ");
    }
}

int main() {
    const char *strings[10] = {
        "User 1", "User 2", "User 3", "User 4", "User 5",
        "User 6", "User 2", "User 8", "User 4", "User 5"
    };

    for (int i = 0; i < 10; i++) {
        uint32_t hash = murmur3_32_string(strings[i], 0x9747b28c);
        printf("Hash of '%s': %u\n", strings[i], hash);
        printf("Binary: ");
        print_binary(hash);
        printf("\n\n");
    }

    return 0;
}
