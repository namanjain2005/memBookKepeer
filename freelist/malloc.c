#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

// how to give back memory ?
// madvise -- MADV_FREE
// page_decay may be ?

// may be if a user ask for small memory
// first try to see if we have it
// if not then we may ask
// but always ask for a bigger memory
//
//
//
// also think about concurrency making it to be a thread or CPU local

// there is also an idea of red black tree

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

struct memMeta {
  size_t size; // size w/o METASIZE
  struct memMeta *next;
  struct memMeta *prev;
  int free;
};

_Static_assert(sizeof(struct memMeta) % ALIGNMENT == 0,
               "METASIZE must be a multiple of ALIGNMENT");

size_t METASIZE = sizeof(struct memMeta);

struct memMeta *freeListHead = NULL;
struct memMeta *freeListTail = NULL;

void *findFreeBlock(size_t size) {
  struct memMeta *currMemBlock = freeListHead;
  while (currMemBlock && !(currMemBlock->free && currMemBlock->size >= size)) {
    currMemBlock = currMemBlock->next;
  }
  return currMemBlock;
}

struct memMeta *requestBlock(size_t size) {
  // Ensure the heap break pointer from the OS is aligned
  uintptr_t cur_brk = (uintptr_t)sbrk(0);
  size_t remainder = cur_brk % ALIGNMENT;
  if (remainder != 0) {
    size_t pad = ALIGNMENT - remainder;
    if (sbrk((intptr_t)pad) == (void *)-1) {
      return NULL;
    }
  }

  struct memMeta *block = (struct memMeta *)sbrk((intptr_t)(size + METASIZE));
  if (block == (void *)-1) {
    return NULL;
  }
  block->free = 0;
  block->next = NULL;
  block->prev = NULL;
  block->size = size;
  return block;
}

void splitBlock(struct memMeta *memBlock, size_t size) {
  struct memMeta *newBlock;
  newBlock = (struct memMeta *)((char *)(memBlock) + METASIZE + size);
  newBlock->next = memBlock->next;
  newBlock->size = memBlock->size - size - METASIZE;
  memBlock->next = newBlock;
  memBlock->size = size;
  newBlock->prev = memBlock;
  newBlock->free = 1;

  if (newBlock->next) {
    newBlock->next->prev = newBlock;
  }

  if (freeListTail == memBlock) {
    freeListTail = newBlock;
  }
}

void *mergeBlock(struct memMeta *memBlock) {
  memBlock->free = 1;

  if (memBlock->prev && memBlock->prev->free) {
    struct memMeta *prev = memBlock->prev;
    prev->size += METASIZE + memBlock->size;
    prev->next = memBlock->next;

    if (memBlock->next) {
      memBlock->next->prev = prev;
    }

    if (memBlock == freeListTail) {
      freeListTail = prev;
    }
    memBlock = prev;
  }

  if (memBlock->next && memBlock->next->free) {
    struct memMeta *next = memBlock->next;
    memBlock->size += METASIZE + next->size;
    memBlock->next = next->next;

    if (next->next) {
      next->next->prev = memBlock;
    }

    if (next == freeListTail) {
      freeListTail = memBlock;
    }
  }
  return memBlock;
}

void *malloc(size_t size) {
  if (size == 0) {
    return NULL;
  }

  if (size > (size_t)-1 - ALIGNMENT - METASIZE) {
    return NULL;
  }

  size_t aligned_size = ALIGN(size);

  if (freeListHead == NULL) {
    // i want this to not exist as it only happens why check every time
    freeListHead = requestBlock(aligned_size);
    if (!freeListHead) {
      return NULL;
    }
    freeListHead->prev = NULL;
    freeListTail = freeListHead;
    return (freeListHead + 1);
  }

  struct memMeta *memBlock = findFreeBlock(aligned_size);
  if (memBlock == NULL) {
    memBlock = requestBlock(aligned_size);
    if (!memBlock) {
      return NULL;
    }
    memBlock->prev = freeListTail;
    freeListTail->next = memBlock;
    freeListTail = memBlock;
    return (memBlock + 1);
  }

  memBlock->free = 0;
  if (memBlock->size >= aligned_size + METASIZE + ALIGNMENT) {
    splitBlock(memBlock, aligned_size);
  }

  return (memBlock + 1);
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }
  // Validate that ptr is properly aligned
  if ((uintptr_t)ptr % ALIGNMENT != 0) {
    return;
  }
  struct memMeta *memBlock = (((struct memMeta *)ptr) - 1);
  memBlock = mergeBlock(memBlock);
  if (memBlock == freeListTail) {
    freeListTail = freeListTail->prev;
    if (freeListTail) {
      freeListTail->next = NULL;
    } else {
      freeListHead = NULL;
    }
    sbrk(-(intptr_t)(memBlock->size + METASIZE));
  }
}
