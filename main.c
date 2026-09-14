#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Declarations of custom malloc and free
extern void *malloc(size_t size);
extern void free(void *ptr);

void test_single_allocation() {
    printf("[Test 1] Basic Single Allocation\n");
    char *ptr = (char *)malloc(100);
    if (ptr == NULL) {
        printf(" -> FAIL: malloc(100) returned NULL.\n\n");
        return;
    }

    printf(" -> Allocated 100 bytes at address: %p\n", (void *)ptr);
    const char *test_str = "Testing custom malloc memory write/read";
    strcpy(ptr, test_str);
    if (strcmp(ptr, test_str) == 0) {
        printf(" -> PASS: Memory write & read verified successfully: \"%s\"\n", ptr);
    } else {
        printf(" -> FAIL: Written data did not match read data.\n");
    }
    printf("\n");
}

void test_multiple_allocations() {
    printf("[Test 2] Multiple Sequential Allocations Data Integrity\n");
    int *arr1 = (int *)malloc(5 * sizeof(int));
    int *arr2 = (int *)malloc(5 * sizeof(int));

    printf(" -> Allocation 1 address: %p\n", (void *)arr1);
    printf(" -> Allocation 2 address: %p\n", (void *)arr2);

    if (arr1 == NULL || arr2 == NULL) {
        printf(" -> FAIL: One or both allocations returned NULL.\n\n");
        return;
    }

    if (arr1 == arr2 || (char *)arr2 < (char *)arr1 + (5 * sizeof(int))) {
        printf(" -> FAIL: Overlapping memory locations detected!\n\n");
        return;
    }

    // Fill data into both arrays
    for (int i = 0; i < 5; i++) {
        arr1[i] = (i + 1) * 111;
        arr2[i] = (i + 1) * 999;
    }

    // Verify data integrity
    int pass = 1;
    for (int i = 0; i < 5; i++) {
        if (arr1[i] != (i + 1) * 111 || arr2[i] != (i + 1) * 999) {
            pass = 0;
            break;
        }
    }

    if (pass) {
        printf(" -> PASS: Sequential allocations hold isolated, valid data without corruption.\n");
    } else {
        printf(" -> FAIL: Data corruption between sequential allocations.\n");
    }
    printf("\n");
}

void test_free_and_reuse() {
    printf("[Test 3] Selective Free & First-Fit Block Reuse\n");
    void *b1 = malloc(64);
    void *b2 = malloc(128);
    void *b3 = malloc(64);

    printf(" -> Allocated b1 (64 bytes)  at: %p\n", b1);
    printf(" -> Allocated b2 (128 bytes) at: %p\n", b2);
    printf(" -> Allocated b3 (64 bytes)  at: %p\n", b3);

    // Free middle block b2
    printf(" -> Freeing middle block b2 (%p)...\n", b2);
    free(b2);

    // Request 100 bytes (fits inside freed b2 of 128 bytes)
    void *b2_reuse = malloc(100);
    printf(" -> Requested 100 bytes (b2_reuse) at: %p\n", b2_reuse);

    if (b2_reuse == b2) {
        printf(" -> PASS: Freed middle block was successfully reused!\n");
    } else {
        printf(" -> FAIL: Freed block was not reused (allocated at new location %p).\n", b2_reuse);
    }

    // Request 200 bytes (larger than freed b2, should create new block)
    void *b4_large = malloc(200);
    printf(" -> Requested 200 bytes (b4_large) at: %p\n", b4_large);
    if (b4_large != b2 && b4_large != b1 && b4_large != b3) {
        printf(" -> PASS: Large request correctly bypassed smaller freed block.\n");
    } else {
        printf(" -> FAIL: Allocation reused a block that was too small!\n");
    }
    printf("\n");
}

void test_zero_allocation() {
    printf("[Test 4] Edge Case: malloc(0)\n");
    void *ptr = malloc(0);
    if (ptr == NULL) {
        printf(" -> PASS: malloc(0) returned NULL.\n");
    } else {
        printf(" -> INFO: malloc(0) returned pointer %p.\n", ptr);
    }
    printf("\n");
}

int main() {
    printf("===================================================\n");
    printf("        TESTING CUSTOM MALLOC (malloc.c)           \n");
    printf("===================================================\n\n");

    test_single_allocation();
    test_multiple_allocations();
    test_free_and_reuse();
    test_zero_allocation();

    printf("===================================================\n");
    printf("               ALL TESTS COMPLETED                 \n");
    printf("===================================================\n");

    return 0;
}
