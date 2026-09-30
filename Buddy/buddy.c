#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

struct buddyBlock {
  size_t len;
  int is_free;
};

_Static_assert(sizeof(struct buddyBlock) % ALIGNMENT == 0,
               "buddyBlock must be a multiple of ALIGNMENT");

struct buddy {
  void *base;
  size_t size;
  struct buddyBlock *head;
  struct buddyBlock *tail;
};

static size_t nextPow2(size_t size) {
  if (size < sizeof(struct buddyBlock)) {
    size = sizeof(struct buddyBlock);
  }
  size_t p = 1;
  while (p < size) {
    p <<= 1;
    if (p == 0) {
      return 0; // overflow
    }
  }
  return p;
}

struct buddyBlock *buddyNextBlock(struct buddyBlock *block) {
  return (struct buddyBlock *)((char *)block + block->len);
}

struct buddyBlock *splitBlock(struct buddyBlock *block, size_t size) {
  // assumption size is power of 2
  while (block->len > size) {
    size_t sz = block->len >> 1;
    block->len = sz;
    struct buddyBlock *buddy = buddyNextBlock(block);
    buddy->len = sz;
    buddy->is_free = 1;
  }

  if (size <= block->len) {
    block->is_free = 0;
    return block;
  }
  // we f'ed up
  return NULL;
}

void *findBestBuddy(struct buddyBlock *head, struct buddyBlock *tail, size_t size) {
  struct buddyBlock *block = head;
  struct buddyBlock *bestBlock = NULL; // as using bestBlock strategy

  while (block < tail) {
    if (block->len == 0) {
      break;
    }
    struct buddyBlock *buddy = buddyNextBlock(block);

    // coalesce
    size_t offset = (size_t)((char *)block - (char *)head);
    if (block->is_free && buddy < tail && buddy->is_free &&
        block->len == buddy->len && (offset % (2 * block->len) == 0)) {
      block->len <<= 1;
      // Re-examine this block as it might coalesce further
      continue;
    }

    if (block->is_free && block->len >= size &&
        (bestBlock == NULL || block->len < bestBlock->len)) {
      bestBlock = block;
    }

    block = buddyNextBlock(block);
  }

  if (bestBlock != NULL) {
    return splitBlock(bestBlock, size);
  }
  return NULL;
}

void buddyFreeBlock(struct buddyBlock *block, struct buddyBlock *head, struct buddyBlock *tail) {
  if (block == NULL || head == NULL || tail == NULL) {
    return;
  }
  if (block < head || block >= tail) {
    return;
  }

  block->is_free = 1;
  size_t totalSize = (size_t)((char *)tail - (char *)head);

  // Coalesce with buddy if possible
  while (block->len < totalSize) {
    size_t offset = (size_t)((char *)block - (char *)head);
    size_t buddyOffset = offset ^ block->len;
    struct buddyBlock *buddy = (struct buddyBlock *)((char *)head + buddyOffset);

    // Validate buddy bounds
    if ((char *)buddy < (char *)head || (char *)buddy >= (char *)tail) {
      break;
    }
    // Buddy must be free and the same size
    if (!buddy->is_free || buddy->len != block->len) {
      break;
    }

    // Merge: block with smaller address retains the coalesced range
    if (buddy < block) {
      block = buddy;
    }
    block->len <<= 1;
    block->is_free = 1;
  }
}

int buddyInit(struct buddy *buddy, size_t size) {
  if (buddy == NULL || size == 0) {
    return -1;
  }

  size = nextPow2(size);
  if (size == 0) {
    return -1;
  }

  void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED) {
    return -1;
  }

  buddy->base = mem;
  buddy->size = size;
  buddy->head = (struct buddyBlock *)mem;
  buddy->tail = (struct buddyBlock *)((char *)mem + size);

  buddy->head->len = size;
  buddy->head->is_free = 1;

  return 0;
}

void *buddyAlloc(struct buddy *buddy, size_t size) {
  if (buddy == NULL || size == 0) {
    return NULL;
  }

  // Account for header metadata and round up to power of 2
  size_t totalNeeded = size + sizeof(struct buddyBlock);
  size_t reqSize = nextPow2(totalNeeded);
  if (reqSize == 0 || reqSize > buddy->size) {
    return NULL;
  }

  struct buddyBlock *block = (struct buddyBlock *)findBestBuddy(buddy->head, buddy->tail, reqSize);
  if (block == NULL) {
    return NULL;
  }

  block->is_free = 0;
  return (void *)(block + 1);
}

void buddyFree(struct buddy *buddy, void *ptr) {
  if (buddy == NULL || ptr == NULL) {
    return;
  }

  if ((char *)ptr < (char *)buddy->head || (char *)ptr >= (char *)buddy->tail) {
    return;
  }

  struct buddyBlock *block;
  size_t ptrOffset = (size_t)((char *)ptr - (char *)buddy->head);
  if (ptrOffset >= sizeof(struct buddyBlock)) {
    struct buddyBlock *candidate = (struct buddyBlock *)ptr - 1;
    size_t candOffset = (size_t)((char *)candidate - (char *)buddy->head);
    if (candidate->len >= sizeof(struct buddyBlock) &&
        (candOffset % candidate->len == 0) && !candidate->is_free) {
      block = candidate;
    } else {
      block = (struct buddyBlock *)ptr;
    }
  } else {
    block = (struct buddyBlock *)ptr;
  }

  buddyFreeBlock(block, buddy->head, buddy->tail);
}

void buddyDestroy(struct buddy *buddy) {
  if (buddy == NULL) {
    return;
  }
  if (buddy->base != NULL) {
    munmap(buddy->base, buddy->size);
  }
  buddy->base = NULL;
  buddy->size = 0;
  buddy->head = NULL;
  buddy->tail = NULL;
}
