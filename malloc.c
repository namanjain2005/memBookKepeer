#include <stddef.h>
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

struct memMeta {
  size_t size; // size w/o METASIZE
  struct memMeta *next;
  struct memMeta *prev;
  int free;
};
size_t METASIZE = sizeof(struct memMeta);

struct memMeta *freeListHead = NULL;
struct memMeta *freeListTail = NULL;
// i dont want this to be ticks becoz what if i never touch this modules

void *findFreeBlock(size_t size) {
  struct memMeta *currMemBlock = freeListHead;
  while (currMemBlock && !(currMemBlock->free && currMemBlock->size >= size)) {
    currMemBlock = currMemBlock->next;
  }
  return currMemBlock;
}

struct memMeta *requestBlock(size_t size) {
  struct memMeta *block = sbrk(0);
  void *mem = sbrk(size + METASIZE);
  if (mem == (void *)-1) {
    return NULL;
  }
  block->free = 0;
  block->next = NULL;
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
  if (size <= 0) {
    return NULL;
  }

  if (freeListHead == NULL) {
    // i want this to not exist as it only happens why check every time
    freeListHead = requestBlock(size);
    freeListHead->prev = NULL;
    freeListTail = freeListHead;
    return (freeListHead + 1);
  }

  struct memMeta *memBlock = findFreeBlock(size);
  if (memBlock == NULL) {
    memBlock = requestBlock(size);
    memBlock->prev = freeListTail;
    freeListTail->next = memBlock;
    freeListTail = memBlock;
    return (memBlock + 1);
  }

  memBlock->free = 0;
  if (memBlock->size >= size + (METASIZE + 4)) {
    splitBlock(memBlock, size);
  }

  return (memBlock + 1);
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }
  // TODO -- what if ptr does not align properly ? may be some validation
  struct memMeta *memBlock = (((struct memMeta *)ptr) - 1);
  memBlock = mergeBlock(memBlock);
  if (memBlock == freeListTail) {
    freeListTail = freeListTail->prev;
    if (freeListTail) {
      freeListTail->next = NULL;
    } else {
      freeListHead = NULL;
    }
    sbrk(-(memBlock->size + METASIZE));
  }
}

// void* realloc(ptr, sizeof(int[69]));
