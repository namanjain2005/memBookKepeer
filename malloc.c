#include <stddef.h>
#include <unistd.h>

struct memMeta {
  size_t size;
  struct memMeta *next;
  struct memMeta *prev;
  int free;
};
size_t METASIZE = sizeof(struct memMeta);

struct memMeta *FreeListHead = NULL;
struct memMeta *FreeListTail = NULL;

void *findFreeBlock(size_t size) {
  struct memMeta *currMemBlock = FreeListHead;
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

  if (FreeListTail == memBlock) {
    FreeListTail = newBlock;
  }
}
void mergeBlock(struct memMeta *memBlock) {
  memBlock->free = 1;

  if (memBlock->prev && memBlock->prev->free) {
    struct memMeta *prev = memBlock->prev;
    prev->size += METASIZE + memBlock->size;
    prev->next = memBlock->next;

    if (memBlock->next) {
      memBlock->next->prev = prev;
    }

    if (memBlock == FreeListTail) {
      FreeListTail = prev;
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

    if (next == FreeListTail) {
      FreeListTail = memBlock;
    }
  }
}

void *malloc(size_t size) {
  if (size <= 0) {
    return NULL;
  }

  if (FreeListHead == NULL) {
    // i want this to not exist as it only happens why check every time
    FreeListHead = requestBlock(size);
    FreeListHead->prev = NULL;
    FreeListTail = FreeListHead;
    return (FreeListHead + 1);
  }

  struct memMeta *memBlock = findFreeBlock(size);
  if (memBlock == NULL) {
    memBlock = requestBlock(size);
    memBlock->prev = FreeListTail;
    FreeListTail->next = memBlock;
    FreeListTail = memBlock;
    return (memBlock + 1);
  }

  if (memBlock->size >= size + (METASIZE + 4)) {
    memBlock->free = 0;
    splitBlock(memBlock, size);
  }

  return (memBlock + 1);
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }
  // TODO -- what if ptr does not alogn properly ? may be some validation
  struct memMeta *memBlock = (((struct memMeta *)ptr) - 1);
  mergeBlock(memBlock);
}

// void* realloc(ptr, sizeof(int[69]));
