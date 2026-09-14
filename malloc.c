#include <stddef.h>
#include <unistd.h>

struct memMeta {
  size_t size;
  struct memMeta *next;
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

void *malloc(size_t size) {
  if (size <= 0) {
    return NULL;
  }

  if (FreeListHead == NULL) {
    // i want this to not exist as it only happens why check every time
    FreeListHead = requestBlock(size);
    FreeListTail = FreeListHead;
    return (FreeListHead + 1);
  }

  struct memMeta *memBlock = findFreeBlock(size);
  if (memBlock == NULL) {
    memBlock = requestBlock(size);
    FreeListTail->next = memBlock;
    FreeListTail = memBlock;
  }
  return (memBlock + 1);
}

void free(void *ptr) {
  if (!ptr) {
    return;
  }
  // TODO -- what if ptr does not alogn properly ? may be some validation
  struct memMeta *memptr = (((struct memMeta *)ptr) - 1);
  memptr->free = 1;
}

// void* realloc(ptr, sizeof(int[69]));
