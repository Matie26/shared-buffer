#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define LIFO 0
#define FIFO 1

struct shared_buffer {
  size_t capacity;
  size_t write;
  size_t read;
  int8_t mode;
  sem_t mutex;
  sem_t empty;
  sem_t full;
  int *buf;
};

void privatewrite(struct shared_buffer *buffer, int data) {
  int *p = buffer->buf;
  if (buffer->mode == FIFO && buffer->write == buffer->capacity - 1) {
    buffer->write = -1;
  }
  p += ++buffer->write;
  *p = data;
}

int privateread(struct shared_buffer *buffer) {
  int *p = buffer->buf;
  switch (buffer->mode) {
  case LIFO:
    p += buffer->write--;
    break;
  case FIFO:
    p += buffer->read;
    if (buffer->read == buffer->capacity - 1) {
      buffer->read = 0;
    } else {
      buffer->read++;
    }
    break;
  }
  int data = *p;
  return data;
}

struct shared_buffer *new_shared_buffer(size_t size, int8_t mode) {
  struct shared_buffer *temp =
      mmap(NULL, sizeof(struct shared_buffer), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  int *shared_mem = mmap(NULL, size * sizeof(int), PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sem_init(&(temp->mutex), 1, 1);
  sem_init(&(temp->empty), 1, size);
  sem_init(&(temp->full), 1, 0);
  temp->capacity = size;
  temp->mode = mode;
  temp->write = -1;
  temp->read = 0;
  temp->buf = shared_mem;
  return temp;
}

void print_shared_buffer(struct shared_buffer *buffer, size_t elements_count) {
  sem_wait(&(buffer->mutex));
  for (size_t i = 0; i < elements_count; i++) {
    printf("%ld element: %d", i, buffer->buf[i]);
    if (buffer->mode == LIFO) {
      if (buffer->write == i)
        printf(" <-- write (read)");
    } else {
      if (buffer->write == i || buffer->read == i) {
        printf(" <-- ");
        if (buffer->write == i)
          printf(" write ");
        else
          printf(" read ");
      }
    }
    printf("\n");
  }
  sem_post(&(buffer->mutex));
}

void add_items(struct shared_buffer *buffer, int *items, size_t items_count) {
  for (size_t i = 0; i < items_count; i++) {
    sem_wait(&(buffer->empty));
    sem_wait(&(buffer->mutex));
    privatewrite(buffer, *(items + i));
    sem_post(&(buffer->mutex));
    sem_post(&(buffer->full));
  }
}

int *get_items(struct shared_buffer *buffer, size_t items_count) {
  int *result = malloc(items_count * sizeof(int));
  for (size_t i = 0; i < items_count; i++) {
    sem_wait(&(buffer->full));
    sem_wait(&(buffer->mutex));
    *(result + i) = privateread(buffer);
    sem_post(&(buffer->mutex));
    sem_post(&(buffer->empty));
  }
  return result;
}

int check_for_duplicates(int *data, size_t size) {
  for (size_t i = 0; i < size - 1; i++) {
    for (size_t j = i + 1; j < size; j++) {
      if (*(data + i) == *(data + j))
        return 1;
    }
  }
  return 0;
}

void producer(struct shared_buffer *buffer, size_t items_count,
              int producer_id) {
  printf("Producer %d starting\n", producer_id);
  int *data = malloc(items_count * sizeof(int));
  for (size_t i = 0; i < items_count; i++) {
    *(data + i) = i * 100 + producer_id;
  }
  add_items(buffer, data, items_count);
  printf("[+] Producer %d finished\n", producer_id);
}

int *consumer(struct shared_buffer *buffer, size_t items_count, int consumer_id,
              int verbose) {
  printf("Consumer %d starting\n", consumer_id);
  int *p = get_items(buffer, items_count);
  if (verbose) {
    printf("Consumer got: ");
    for (size_t i = 0; i < items_count; i++) {
      printf("%d, ", *(p + i));
    }
    printf("\n");
  }
  printf("[+] Consumer %d finished:", consumer_id);
  if (check_for_duplicates(p, items_count))
    printf(" DUPLICATES FOUND!\n");
  else
    printf(" No duplicates found\n");
  return p;
}

void shared_buffer_test(struct shared_buffer *buffer) {
  int n = 5000;
  int *data;
  struct shared_buffer *temp = new_shared_buffer(n, FIFO);
  pid_t pids[10];
  for (int i = 0; i < 10; i++) {
    if ((pids[i] = fork()) < 0) {
      perror("fork");
      abort();
    } else if (pids[i] == 0) {
      if (i % 2 == 0) {
        producer(buffer, n / 5, i);
      } else {
        data = consumer(buffer, n / 5, i, 0);
        add_items(temp, data, n / 5);
      }
      exit(0);
    }
  }
  sleep(3);
  data = get_items(temp, n);
  if (check_for_duplicates(data, n))
    printf("\nDuplicates found!\n");
  else
    printf("\nNo duplicates found!\n");
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Expected 2 arguemnts: ./sharedBuffer buffer_size buffer_mode "
           "[0:LIFO 1:FIFO]\n");
    return 1;
  }
  int buffer_size = atoi(argv[1]);
  int buffer_mode = atoi(argv[2]);
  struct shared_buffer *buffer = new_shared_buffer(buffer_size, buffer_mode);
  shared_buffer_test(buffer);
  return 0;
}