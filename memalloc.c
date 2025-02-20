#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t global_malloc_lock;

typedef char ALIGN[16];

union header {
  struct {
    size_t size;
    unsigned is_free;
    union header *next;
  } s;
  ALIGN stub;
};
typedef union header header_t;

header_t *head = NULL, *tail = NULL;

header_t *get_free_block(size_t size);

void *memalloc(size_t size) {
  size_t total_size;
  void *block;
  header_t *header;
  if (!size)
    return NULL;

  pthread_mutex_lock(&global_malloc_lock);
  header = get_free_block(size);

  if (header) {
    header->s.is_free = 0;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
  }

  total_size = sizeof(header_t) + size;
  block = sbrk(total_size);
  if (block == (void *)-1) {
    pthread_mutex_unlock(&global_malloc_lock);
    return NULL;
  }

  header = block;
  header->s.size = size;
  header->s.is_free = 0;
  header->s.next = NULL;

  if (!head)
    head = header;
  if (tail)
    tail->s.next = header;
  tail = header;

  pthread_mutex_unlock(&global_malloc_lock);
  return (void *)(header + 1);
}

header_t *get_free_block(size_t size) {
  header_t *curr = head;
  while (curr) {
    if (curr->s.is_free && curr->s.size >= size)
      return curr;
    curr = curr->s.next;
  }
  return NULL;
}

void free(void *block) {
  header_t *header, *tmp;
  void *programbreak;

  if (!block)
    return;

  pthread_mutex_unlock(&global_malloc_lock);
  header = (header_t *)block - 1;

  programbreak = sbrk(0);
  if ((char *)block + header->s.size == programbreak) {
    if (head == tail) {
      head = tail = NULL;
    } else {
      tmp = head;
      while (tmp) {
        if (tmp->s.next == tail) {
          tmp->s.next = NULL;
          tail = tmp;
        }
        tmp = tmp->s.next;
      }
    }
    sbrk(0 - sizeof(header_t) - header->s.size);
    pthread_mutex_unlock(&global_malloc_lock);
    return;
  }
  header->s.is_free = 1;
  pthread_mutex_unlock(&global_malloc_lock);
}

void *mecalloc(size_t num, size_t nsize) {
  size_t size;
  void *block;

  if (!num || !nsize)
    return NULL;

  size = num * nsize;
  if (nsize != size / num) // check multiplication overflow
    return NULL;

  block = memalloc(size);
  if (!block)
    return NULL;

  memset(block, 0, size);
  return block;
}

int main(void) {
  int *num = memalloc(sizeof(int));
  printf("before initializing: %d\n", *num);

  *num = 55;
  printf("after initializing: %d\n", *num);

  int *nums = mecalloc(5, sizeof(int));
  for (int i = 0; i < 5; i++)
    nums[i] = i * 5;
  for (int i = 0; i < 5; i++)
    printf("%d\n", nums[i]);
  free(num);
  free(nums);
}
