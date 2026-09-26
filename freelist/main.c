#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

struct memMeta {
  size_t size; // size w/o METASIZE
  struct memMeta *next;
  struct memMeta *prev;
  int free;
};

// Declarations of custom malloc, free, and METASIZE
extern void *malloc(size_t size);
extern void free(void *ptr);
extern size_t METASIZE;

void test_basic_allocation() {
  printf("[Test 1] Basic Allocation & Memory Write/Read\n");
  char *ptr = (char *)malloc(128);
  if (!ptr) {
    printf(" -> FAIL: malloc returned NULL.\n\n");
    return;
  }
  strcpy(ptr, "Malloc Test");
  if (strcmp(ptr, "Malloc Test") == 0) {
    printf(" -> PASS: Memory successfully allocated and verified.\n");
  } else {
    printf(" -> FAIL: Data read mismatch.\n");
  }
  free(ptr);
  printf("\n");
}

void test_multiple_allocations() {
  printf("[Test 2] Multiple Sequential Allocations\n");
  int *a = (int *)malloc(10 * sizeof(int));
  int *b = (int *)malloc(10 * sizeof(int));

  if (!a || !b) {
    printf(" -> FAIL: Allocation failed.\n\n");
    return;
  }

  for (int i = 0; i < 10; i++) {
    a[i] = i + 1;
    b[i] = (i + 1) * 10;
  }

  int ok = 1;
  for (int i = 0; i < 10; i++) {
    if (a[i] != i + 1 || b[i] != (i + 1) * 10) {
      ok = 0;
      break;
    }
  }

  if (ok) {
    printf(
        " -> PASS: Multiple allocations hold distinct data without overlap.\n");
  } else {
    printf(" -> FAIL: Data corruption detected across allocations.\n");
  }
  printf("\n");
}

void test_free_and_reuse() {
  printf("[Test 3] Block Reuse\n");
  void *p1 = malloc(64);
  void *p2 = malloc(128);
  void *p3 = malloc(64);

  (void)p1;
  (void)p3;

  uintptr_t p2_addr = (uintptr_t)p2;
  free(p2);
  void *p_reuse = malloc(100);

  if ((uintptr_t)p_reuse == p2_addr) {
    printf(
        " -> PASS: Freed block (p2) was reused for subsequent allocation.\n");
  } else {
    printf(" -> FAIL: Freed block was not reused (allocated at %p instead of "
           "0x%lx).\n",
           p_reuse, p2_addr);
  }
  printf("\n");
}

void test_edge_cases() {
  printf("[Test 4] Edge Cases: malloc(0) and free(NULL)\n");
  void *p0 = malloc(0);
  if (p0 == NULL) {
    printf(" -> PASS: malloc(0) correctly returned NULL.\n");
  } else {
    printf(" -> INFO: malloc(0) returned non-NULL pointer %p.\n", p0);
  }

  // free(NULL) should execute safely without crashing
  free(NULL);
  printf(" -> PASS: free(NULL) executed safely.\n\n");
}

void test_tail_free_sbrk() {
  printf("[Test 5] Tail-End Memory Release via sbrk() with Alignment\n");

  void *brk_before = sbrk(0);
  void *p1 = malloc(100);
  void *p2 = malloc(200);
  void *brk_allocated = sbrk(0);

  size_t s1 = ALIGN(100);
  size_t s2 = ALIGN(200);
  size_t expected_expansion = s1 + s2 + 2 * METASIZE;

  if ((char *)brk_allocated != (char *)brk_before + expected_expansion) {
    printf(" -> FAIL: did not expand heap properly %p expected %p.\n\n",
           brk_allocated, (char *)brk_before + expected_expansion);
    return;
  }

  free(p2);
  void *brk_after_p2_free = sbrk(0);

  size_t expected_shrink_p2 = s2 + METASIZE;
  if ((size_t)((char *)brk_allocated - (char *)brk_after_p2_free) == expected_shrink_p2) {
    printf(" -> PASS: Tail block free reduced heap break by exact offset (%zu "
           "bytes = ALIGN(200) + METASIZE).\n",
           expected_shrink_p2);
  } else {
    printf(" -> FAIL: Expected shrink of %zu bytes, but got %td bytes "
           "difference.\n",
           expected_shrink_p2,
           (char *)brk_allocated - (char *)brk_after_p2_free);
  }

  free(p1);
  void *brk_after_p1_free = sbrk(0);
  size_t expected_shrink_p1 = s1 + METASIZE;
  if ((size_t)((char *)brk_after_p2_free - (char *)brk_after_p1_free) ==
      expected_shrink_p1) {
    printf(" -> PASS: Freeing remaining block reduced heap break by exact "
           "offset (%zu bytes = ALIGN(100) + METASIZE).\n",
           expected_shrink_p1);
  } else {
    printf(" -> FAIL: Expected shrink of %zu bytes for p1, but got %td bytes "
           "difference.\n",
           expected_shrink_p1,
           (char *)brk_after_p2_free - (char *)brk_after_p1_free);
  }

  printf("\n");
}

void test_tail_free_cascading_merge() {
  printf("[Test 6] Cascading Merge and Tail Release with Alignment\n");

  void *p1 = malloc(100);
  void *p2 = malloc(200);
  void *p3 = malloc(300);

  void *brk_allocated = sbrk(0);
  free(p2);
  free(p3);
  void *brk_after_merge_free = sbrk(0);

  size_t s2 = ALIGN(200);
  size_t s3 = ALIGN(300);
  size_t expected_total_shrink = s3 + s2 + 2 * METASIZE;
  if ((size_t)((char *)brk_allocated - (char *)brk_after_merge_free) ==
      expected_total_shrink) {
    printf(" -> PASS: Cascading merge reduced heap break by exact merged "
           "offset (%zu bytes = ALIGN(300) + ALIGN(200) + 2*METASIZE).\n",
           expected_total_shrink);
  } else {
    printf(" -> FAIL: Expected merged shrink of %zu bytes, but got %td bytes "
           "difference.\n",
           expected_total_shrink,
           (char *)brk_allocated - (char *)brk_after_merge_free);
  }

  free(p1);
  printf("\n");
}

void test_non_tail_free_preserves_brk() {
  printf("[Test 7] Non-Tail Free Preserves Heap Break\n");
  void *p1 = malloc(100);
  void *p2 = malloc(200);
  void *brk_after_alloc = sbrk(0);

  // Freeing non-tail block p1 should mark it free but NOT decrease sbrk break
  free(p1);
  void *brk_after_p1_free = sbrk(0);

  if (brk_after_p1_free == brk_after_alloc) {
    printf(" -> PASS: Non-tail free preserved heap break as expected.\n");
  } else {
    printf(" -> FAIL: Heap break changed unexpectedly when freeing non-tail "
           "block.\n");
  }

  free(p2);
  printf("\n");
}

void test_alignment_various_sizes() {
  printf("[Test 8] Memory Alignment Across Various Request Sizes\n");
  size_t sizes[] = {1, 2, 3, 5, 8, 13, 16, 17, 24, 31, 32, 33, 47, 63, 64, 65, 99, 100, 127, 128, 255, 256, 511, 1024};
  size_t n = sizeof(sizes) / sizeof(sizes[0]);
  int pass = 1;

  for (size_t i = 0; i < n; i++) {
    size_t sz = sizes[i];
    void *ptr = malloc(sz);
    if (!ptr) {
      printf(" -> FAIL: malloc(%zu) returned NULL\n", sz);
      pass = 0;
      break;
    }

    // Verify pointer is aligned to ALIGNMENT (16 bytes)
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
      printf(" -> FAIL: malloc(%zu) returned unaligned pointer %p\n", sz, ptr);
      pass = 0;
      free(ptr);
      break;
    }

    // Verify metadata block is aligned to ALIGNMENT
    struct memMeta *meta = ((struct memMeta *)ptr) - 1;
    if ((uintptr_t)meta % ALIGNMENT != 0) {
      printf(" -> FAIL: malloc(%zu) metadata at %p is not aligned\n", sz, (void *)meta);
      pass = 0;
      free(ptr);
      break;
    }

    // Verify meta->size is at least aligned size and multiple of ALIGNMENT
    if (meta->size < ALIGN(sz) || meta->size % ALIGNMENT != 0) {
      printf(" -> FAIL: malloc(%zu) meta->size = %zu is not aligned\n", sz, meta->size);
      pass = 0;
      free(ptr);
      break;
    }

    // Write pattern to verify entire buffer is writable
    memset(ptr, 0xA5, sz);
    unsigned char *uptr = (unsigned char *)ptr;
    for (size_t j = 0; j < sz; j++) {
      if (uptr[j] != 0xA5) {
        printf(" -> FAIL: Data mismatch at byte %zu for size %zu\n", j, sz);
        pass = 0;
        break;
      }
    }

    free(ptr);
    if (!pass) break;
  }

  if (pass) {
    printf(" -> PASS: All %zu allocations were strictly %d-byte aligned and writable.\n", n, ALIGNMENT);
  }
  printf("\n");
}

void test_os_break_alignment() {
  printf("[Test 9] OS Heap Break Alignment Invariant\n");
  int pass = 1;

  uintptr_t brk_start = (uintptr_t)sbrk(0);
  if (brk_start % ALIGNMENT != 0) {
    printf(" -> FAIL: Initial sbrk(0) %p is not aligned to %d bytes\n", (void *)brk_start, ALIGNMENT);
    pass = 0;
  }

  void *ptrs[10];
  for (int i = 0; i < 10; i++) {
    ptrs[i] = malloc(17 * (i + 1));
    uintptr_t cur_brk = (uintptr_t)sbrk(0);
    if (cur_brk % ALIGNMENT != 0) {
      printf(" -> FAIL: sbrk(0) %p after malloc(%d) is not aligned\n", (void *)cur_brk, 17 * (i + 1));
      pass = 0;
    }
  }

  for (int i = 9; i >= 0; i--) {
    free(ptrs[i]);
    uintptr_t cur_brk = (uintptr_t)sbrk(0);
    if (cur_brk % ALIGNMENT != 0) {
      printf(" -> FAIL: sbrk(0) %p after free(ptrs[%d]) is not aligned\n", (void *)cur_brk, i);
      pass = 0;
    }
  }

  if (pass) {
    printf(" -> PASS: Heap break remained strictly %d-byte aligned throughout operations.\n", ALIGNMENT);
  }
  printf("\n");
}

void test_split_block_alignment() {
  printf("[Test 10] Split Block Alignment and Remainder Alignment\n");
  // Allocate a large block
  void *large = malloc(512);
  if (!large) {
    printf(" -> FAIL: Failed to allocate large block\n\n");
    return;
  }

  // Anchor the heap so large is not at the tail, preventing sbrk release on free
  void *anchor = malloc(64);

  free(large);

  // Allocate a smaller block that triggers split
  void *small = malloc(45);
  int pass = 1;

  if ((uintptr_t)small % ALIGNMENT != 0) {
    printf(" -> FAIL: small block %p is not aligned\n", small);
    pass = 0;
  }

  struct memMeta *small_meta = ((struct memMeta *)small) - 1;
  if ((uintptr_t)small_meta % ALIGNMENT != 0) {
    printf(" -> FAIL: small_meta %p is not aligned\n", (void *)small_meta);
    pass = 0;
  }

  struct memMeta *remainder = small_meta->next;
  if (!remainder || !remainder->free) {
    printf(" -> FAIL: Remainder block not found or not marked free\n");
    pass = 0;
  } else {
    if ((uintptr_t)remainder % ALIGNMENT != 0) {
      printf(" -> FAIL: remainder metadata %p is not aligned\n", (void *)remainder);
      pass = 0;
    }
    void *remainder_payload = (void *)(remainder + 1);
    if ((uintptr_t)remainder_payload % ALIGNMENT != 0) {
      printf(" -> FAIL: remainder payload %p is not aligned\n", remainder_payload);
      pass = 0;
    }
    if (remainder->size % ALIGNMENT != 0) {
      printf(" -> FAIL: remainder size %zu is not a multiple of %d\n", remainder->size, ALIGNMENT);
      pass = 0;
    }
  }

  // Allocate from the remainder
  void *from_rem = malloc(64);
  if ((uintptr_t)from_rem % ALIGNMENT != 0) {
    printf(" -> FAIL: allocation from remainder %p is not aligned\n", from_rem);
    pass = 0;
  }

  free(small);
  free(from_rem);
  free(anchor);

  if (pass) {
    printf(" -> PASS: Split block, remainder header, and remainder payload all maintained %d-byte alignment.\n", ALIGNMENT);
  }
  printf("\n");
}

void test_free_unaligned_pointer() {
  printf("[Test 11] Unaligned Pointer Validation on free()\n");
  void *ptr = malloc(64);
  if (!ptr) {
    printf(" -> FAIL: malloc(64) returned NULL\n\n");
    return;
  }

  // Attempt to free unaligned pointers; free() should safely ignore them
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
  free((void *)((char *)ptr + 1));
  free((void *)((char *)ptr + 3));
  free((void *)((char *)ptr + 7));
#pragma GCC diagnostic pop

  // The original block should still be valid and writable
  memset(ptr, 0x42, 64);
  int ok = 1;
  unsigned char *uptr = (unsigned char *)ptr;
  for (int i = 0; i < 64; i++) {
    if (uptr[i] != 0x42) {
      ok = 0;
      break;
    }
  }

  if (ok) {
    printf(" -> PASS: free() safely ignored unaligned pointers without corrupting memory.\n");
  } else {
    printf(" -> FAIL: Memory corrupted after freeing unaligned pointer.\n");
  }

  free(ptr);
  printf("\n");
}

void test_max_align_compatibility() {
  printf("[Test 12] Standard Types & Maximum Alignment Compatibility\n");
  typedef struct {
    long double ld;
    uint64_t u64;
    void *ptr;
  } MaxAlignedStruct;

  MaxAlignedStruct *s = (MaxAlignedStruct *)malloc(sizeof(MaxAlignedStruct));
  if (!s) {
    printf(" -> FAIL: malloc failed for MaxAlignedStruct\n\n");
    return;
  }

  if ((uintptr_t)s % _Alignof(MaxAlignedStruct) != 0) {
    printf(" -> FAIL: Pointer %p is not aligned to _Alignof(MaxAlignedStruct) = %zu\n",
           (void *)s, _Alignof(MaxAlignedStruct));
    free(s);
    return;
  }

  s->ld = 3.14159265358979323846L;
  s->u64 = 0x123456789ABCDEF0ULL;
  s->ptr = s;

  if (s->u64 == 0x123456789ABCDEF0ULL && s->ptr == s) {
    printf(" -> PASS: MaxAlignedStruct (alignment requirement %zu) successfully allocated and accessed.\n",
           _Alignof(MaxAlignedStruct));
  } else {
    printf(" -> FAIL: Data mismatch in MaxAlignedStruct.\n");
  }

  free(s);
  printf("\n");
}

void test_consecutive_allocations_alignment() {
  printf("[Test 13] Consecutive Mixed-Size Allocations Alignment & Non-Overlap\n");
  #define NUM_BLOCKS 25
  void *blocks[NUM_BLOCKS];
  size_t block_sizes[NUM_BLOCKS];

  int pass = 1;
  for (int i = 0; i < NUM_BLOCKS; i++) {
    block_sizes[i] = (i * 19 + 7) % 256 + 1; // odd, irregular sizes
    blocks[i] = malloc(block_sizes[i]);
    if (!blocks[i]) {
      printf(" -> FAIL: malloc failed at index %d\n", i);
      pass = 0;
      break;
    }
    if ((uintptr_t)blocks[i] % ALIGNMENT != 0) {
      printf(" -> FAIL: Block %d at %p is not %d-byte aligned\n", i, blocks[i], ALIGNMENT);
      pass = 0;
    }
    // Write unique byte
    memset(blocks[i], (unsigned char)(i + 1), block_sizes[i]);
  }

  // Verify contents and non-overlap
  for (int i = 0; i < NUM_BLOCKS && pass; i++) {
    unsigned char *bytes = (unsigned char *)blocks[i];
    for (size_t j = 0; j < block_sizes[i]; j++) {
      if (bytes[j] != (unsigned char)(i + 1)) {
        printf(" -> FAIL: Overlap / corruption detected in block %d at byte %zu\n", i, j);
        pass = 0;
        break;
      }
    }
  }

  // Free all blocks
  for (int i = 0; i < NUM_BLOCKS; i++) {
    if (blocks[i]) {
      free(blocks[i]);
    }
  }

  if (pass) {
    printf(" -> PASS: %d mixed-size allocations verified for %d-byte alignment and memory isolation.\n",
           NUM_BLOCKS, ALIGNMENT);
  }
  printf("\n");
  #undef NUM_BLOCKS
}

int main() {
  printf("===================================================\n");
  printf("        CUSTOM MALLOC & FREE TEST SUITE            \n");
  printf("===================================================\n\n");

  test_basic_allocation();
  test_multiple_allocations();
  test_free_and_reuse();
  test_edge_cases();
  test_tail_free_sbrk();
  test_tail_free_cascading_merge();
  test_non_tail_free_preserves_brk();
  test_alignment_various_sizes();
  test_os_break_alignment();
  test_split_block_alignment();
  test_free_unaligned_pointer();
  test_max_align_compatibility();
  test_consecutive_allocations_alignment();

  printf("===================================================\n");
  printf("               ALL TESTS COMPLETE                  \n");
  printf("===================================================\n");

  return 0;
}
