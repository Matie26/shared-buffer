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
    int* buf;
};

void privatewrite(struct shared_buffer* buffer, int data) {
    int* p = buffer->buf;
    if (buffer->mode == FIFO && buffer->write == buffer->capacity - 1) {
        buffer->write = -1;
    }
    p += ++buffer->write;
    *p = data;
}

int privateread(struct shared_buffer* buffer) {
    int* p = buffer->buf;
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

struct shared_buffer* newSharedBuffer(size_t size, int8_t mode) {
    struct shared_buffer* temp =
        mmap(NULL, sizeof(struct shared_buffer), PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int* sharedMem = mmap(NULL, size * sizeof(int), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(&(temp->mutex), 1, 1);
    sem_init(&(temp->empty), 1, size);
    sem_init(&(temp->full), 1, 0);
    temp->capacity = size;
    temp->mode = mode;
    temp->write = -1;
    temp->read = 0;
    temp->buf = sharedMem;
    return temp;
}

void printSharedBuffer(struct shared_buffer* buffer, size_t nOfElements) {
    sem_wait(&(buffer->mutex));
    for (size_t i = 0; i < nOfElements; i++) {
        printf("%ld element: %d", i, buffer->buf[i]);
        if (buffer->mode == LIFO) {
            if (buffer->write == i) printf(" <-- write (read)");
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

void addItems(struct shared_buffer* buffer, int* items, size_t numberOfItems) {
    for (size_t i = 0; i < numberOfItems; i++) {
        sem_wait(&(buffer->empty));
        sem_wait(&(buffer->mutex));
        privatewrite(buffer, *(items + i));
        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
    }
}

int* getItems(struct shared_buffer* buffer, size_t numberOfItems) {
    int* result = malloc(numberOfItems * sizeof(int));
    for (size_t i = 0; i < numberOfItems; i++) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));
        *(result + i) = privateread(buffer);
        sem_post(&(buffer->mutex));
        sem_post(&(buffer->empty));
    }
    return result;
}

int checkForDups(int* data, size_t size) {
    for (size_t i = 0; i < size - 1; i++) {
        for (size_t j = i + 1; j < size; j++) {
            if (*(data + i) == *(data + j)) return 1;
        }
    }
    return 0;
}

void producer(struct shared_buffer* buffer, size_t dataCount, int producerId) {
    printf("Producer %d starting\n", producerId);
    int* data = malloc(dataCount * sizeof(int));
    for (size_t i = 0; i < dataCount; i++) {
        *(data + i) = i * 100 + producerId;
    }
    addItems(buffer, data, dataCount);
    printf("[+] Producer %d finished\n", producerId);
}

int* consumer(struct shared_buffer* buffer, size_t numberOfItems,
              int consumerId, int verbose) {
    printf("Consumer %d starting\n", consumerId);
    int* p = getItems(buffer, numberOfItems);
    if (verbose) {
        printf("Consumer got: ");
        for (size_t i = 0; i < numberOfItems; i++) {
            printf("%d, ", *(p + i));
        }
        printf("\n");
    }
    printf("[+] Consumer %d finished:", consumerId);
    if (checkForDups(p, numberOfItems))
        printf(" DUPLICATES FOUND!\n");
    else
        printf(" No duplicates found\n");
    return p;
}

void sharedBufferTest(struct shared_buffer* buffer) {
    int n = 5000;
    int* data;
    struct shared_buffer* temp = newSharedBuffer(n, FIFO);
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
                addItems(temp, data, n / 5);
            }
            exit(0);
        }
    }
    sleep(3);
    data = getItems(temp, n);
    if (checkForDups(data, n))
        printf("\nDuplicates found!\n");
    else
        printf("\nNo duplicates found!\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf(
            "Expected 2 arguemnts: ./sharedBuffer bufferSize bufferMode "
            "[0:LIFO 1:FIFO]\n");
        return 1;
    }
    int bufferSize = atoi(argv[1]);
    int bufferMode = atoi(argv[2]);
    struct shared_buffer* buffer = newSharedBuffer(bufferSize, bufferMode);
    sharedBufferTest(buffer);
    return 0;
}