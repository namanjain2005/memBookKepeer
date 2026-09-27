#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

struct arena {
  void *base;
  size_t offset;
  size_t bufferLen;
};

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

int arenaInit(struct arena *arena, size_t capacity) {
  capacity = ALIGN(capacity);
  void *mem = mmap(NULL, capacity, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    return -1;
  }
  arena->base = mem;
  arena->offset = 0;
  arena->bufferLen = capacity;
  return 0;
}

void *arenaAlloc(struct arena *arena, size_t size) {
  size = ALIGN(size);
  if (arena->offset + size > arena->bufferLen) {
    return NULL;
  }
  void *ptr = (char *)arena->base + arena->offset;
  arena->offset += size;
  return ptr;
}

void *arenaRealloc(struct arena *arena, void *oldPtr, size_t oldSize,
                   size_t newSize) {
  if (oldPtr == NULL) {
    return arenaAlloc(arena, newSize);
  }

  size_t alignedOld = ALIGN(oldSize);
  size_t alignedNew = ALIGN(newSize);

  if (alignedNew <= alignedOld) {
    return oldPtr;
  }

  void *frontier = (char *)arena->base + arena->offset;
  void *oldEnd = (char *)oldPtr + alignedOld;
  if (oldEnd == frontier) {
    size_t extra = alignedNew - alignedOld;
    if (arena->offset + extra <= arena->bufferLen) {
      arena->offset += extra;
      return oldPtr;
    }
    return NULL;
  }

  /* General case – allocate + copy */
  void *newPtr = arenaAlloc(arena, newSize);
  if (newPtr == NULL) {
    return NULL;
  }
  memcpy(newPtr, oldPtr, oldSize);
  return newPtr;
}

void arenaReset(struct arena *arena) { arena->offset = 0; }

void arenaDestroy(struct arena *arena) {
  if (arena->base != NULL) {
    munmap(arena->base, arena->bufferLen);
  }
  arena->base = NULL;
  arena->offset = 0;
  arena->bufferLen = 0;
}
