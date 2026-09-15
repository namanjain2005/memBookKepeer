#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main() {
  printf("===================================================\n");
  printf("        CUSTOM MALLOC & FREE TEST SUITE            \n");
  printf("===================================================\n\n");

  test_basic_allocation();
  test_multiple_allocations();
  test_free_and_reuse();
  test_edge_cases();

  printf("===================================================\n");
  printf("               TEST RUN COMPLETE                   \n");
  printf("===================================================\n");

  return 0;
}
