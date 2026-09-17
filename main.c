#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Declarations of custom malloc and free
extern void *malloc(size_t size);
extern void free(void *ptr);

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

  free(p2);
  void *p_reuse = malloc(100);

  if (p_reuse == p2) {
    // interesting this test case might not make sense for some right
    // implementation
    printf(
        " -> PASS: Freed block (p2) was reused for subsequent allocation.\n");
  } else {
    printf(" -> FAIL: Freed block was not reused (allocated at %p instead of "
           "%p).\n",
           p_reuse, p2);
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
  printf("[Test 5] Tail-End Memory Release via sbrk()\n");
  struct memMeta {
    size_t size;
    void *next;
    void *prev;
    int free;
  };
  size_t METASIZE = sizeof(struct memMeta);

  void *brk_before = sbrk(0);
  void *p1 = malloc(100);
  void *p2 = malloc(200);
  void *brk_allocated = sbrk(0);

  if ((char *)brk_allocated != (char *)brk_before + 100 + 200 + 2 * METASIZE) {
    printf(" -> FAIL: did not expand heap properly %p %p %p.\n\n",
           ((char *)brk_before + 100 + 200 + 2 * METASIZE), brk_allocated,
           brk_before);
    return;
  }

  free(p2);
  void *brk_after_p2_free = sbrk(0);

  size_t expected_shrink_p2 = 200 + METASIZE;
  if ((char *)brk_allocated - (char *)brk_after_p2_free == expected_shrink_p2) {
    printf(" -> PASS: Tail block free reduced heap break by exact offset (%zu "
           "bytes = 200 + METASIZE).\n",
           expected_shrink_p2);
  } else {
    printf(" -> FAIL: Expected shrink of %zu bytes, but got %td bytes "
           "difference.\n",
           expected_shrink_p2,
           (char *)brk_allocated - (char *)brk_after_p2_free);
  }

  free(p1);
  void *brk_after_p1_free = sbrk(0);
  size_t expected_shrink_p1 = 100 + METASIZE;
  if ((char *)brk_after_p2_free - (char *)brk_after_p1_free ==
      expected_shrink_p1) {
    printf(" -> PASS: Freeing remaining block reduced heap break by exact "
           "offset (%zu bytes = 100 + METASIZE).\n",
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
  printf("[Test 6] Cascading Merge and Tail Release\n");
  struct memMeta {
    size_t size;
    void *next;
    void *prev;
    int free;
  };
  size_t METASIZE = sizeof(struct memMeta);

  void *p1 = malloc(100);
  void *p2 = malloc(200);
  void *p3 = malloc(300);

  void *brk_allocated = sbrk(0);
  free(p2);
  free(p3);
  void *brk_after_merge_free = sbrk(0);

  size_t expected_total_shrink = 300 + 200 + 2 * METASIZE;
  if ((char *)brk_allocated - (char *)brk_after_merge_free ==
      expected_total_shrink) {
    printf(" -> PASS: Cascading merge reduced heap break by exact merged "
           "offset (%zu bytes = 500 + 2*METASIZE).\n",
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

  printf("===================================================\n");
  printf("               TEST RUN COMPLETE                   \n");
  printf("===================================================\n");

  return 0;
}
