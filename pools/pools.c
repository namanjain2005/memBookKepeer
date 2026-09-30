#include <assert.h>
#include <stddef.h>
#include <sys/mman.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

struct poolNode {
  struct poolNode *next;
};

struct pool {
  size_t chunkSize;
  size_t poolLen;
  unsigned char *base;
  struct poolNode *head;
};

int poolInit(struct pool *pool, size_t chunkCount, size_t chunkSize) {
  if (chunkSize < sizeof(struct poolNode)) {
    chunkSize = sizeof(struct poolNode);
  }
  chunkSize = ALIGN(chunkSize);

  size_t totalSize = chunkCount * chunkSize;

  void *mem = mmap(NULL, totalSize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    return -1;
  }

  pool->base = (unsigned char *)mem;
  pool->poolLen = totalSize;
  pool->chunkSize = chunkSize;
  pool->head = NULL;

  for (size_t i = 0; i < chunkCount; i++) {
    struct poolNode *node = (struct poolNode *)(pool->base + i * chunkSize);
    node->next = pool->head;
    pool->head = node;
  }

  return 0;
}

void poolFreeAll(struct pool *pool) {
  pool->head = NULL;
  size_t chunkCount = pool->poolLen / pool->chunkSize;

  for (size_t i = 0; i < chunkCount; i++) {
    struct poolNode *node =
        (struct poolNode *)(pool->base + i * pool->chunkSize);
    node->next = pool->head;
    pool->head = node;
  }
}

void *poolAlloc(struct pool *pool) {
  void *ptr = pool->head;

  if (pool->head == NULL) {
    assert(0 && "OOM ERROR");
    return NULL;
  }

  pool->head = pool->head->next;
  return ptr;
}

void pool_free(struct pool *pool, void *ptr) {
  void *start = pool->base;
  void *end = pool->base + pool->poolLen;
  if (!(ptr >= start && ptr < end)) {
    assert(0 && "Out Of Bounds");
    return;
  }
  struct poolNode *node = (struct poolNode *)ptr;
  node->next = pool->head;
  pool->head = node;
  return;
}

void pool_destroy(struct pool *pool) {
  if (pool->base != NULL) {
    munmap(pool->base, pool->poolLen);
  }
  pool->base = NULL;
  pool->poolLen = 0;
  pool->chunkSize = 0;
  pool->head = NULL;
}
