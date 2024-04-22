#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

int shm;

struct Buffer
{
    int added;
    int extracted;
    sem_t *mutex;
    sem_t *mutexL;
    int capacity;
    int begin;
    int end;
    char data[];
};

void *smalloc(int size)
{
    void *block = mmap(
        NULL,
        size + sizeof(size),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0);
    *((int *)block) = size;
    return (char *)block + sizeof(size);
}

void sfree(void *shared)
{
    void *block = (char *)shared - sizeof(int);
    munmap(block, *((int *)block));
}

static struct Buffer *createBuffer(struct Buffer *buffer, int size)
{

    // if ((shm = shm_open("mem", (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR))) == -1)
    // {
    //     perror("shm_open");
    //     return 1;
    // }
    // if (ftruncate(shm, sizeof(struct Buffer)) == -1)
    // {
    //     perror("ftruncate");
    //     return 1;
    // }

    // if (close(shm))
    // {
    //     perror("close");
    //     exit(EXIT_FAILURE);
    // }
    sem_unlink("/mutex");
    sem_unlink("/mutexL");
    *buffer = (struct Buffer){
        .added = 0,
        .extracted = 0,
        .capacity = size,
        .begin = 0,
        .end = 0,
        .mutex = sem_open("/mutex", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR, 1),
        .mutexL = sem_open("/mutexL", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR, 1)};

    return buffer;
}

static inline void freeDesctruct(struct Buffer *buffer)
{
    sfree(buffer);
    sem_unlink("/mutex");
    sem_unlink("/mutexL");
}

static inline int length(struct Buffer *buffer)
{
    sem_wait(buffer->mutexL);
    int length = buffer->begin <= buffer->end
                     ? buffer->end - buffer->begin
                     : ((buffer->end - 0) + (buffer->capacity - buffer->begin));
    sem_post(buffer->mutexL);
    return length;
}

// static inline int capacity(struct Buffer *buffer) { return buffer->end - buffer->begin; }

// static inline void moveHead(struct Buffer *buffer)
// {
//     buffer->head++;
//     if (buffer->head >= buffer->end)
//         buffer->head = buffer->begin;
// }

// static inline void moveTail(struct Buffer *buffer)
// {
//     buffer->tail++;
//     if (buffer->tail >= buffer->end)
//         buffer->tail = buffer->begin;
// }

// int bytesSize(uint8_t messageSize) { return (messageSize + 3) / 4 * 4; }

int availableBuffer(struct Buffer *buffer)
{
    return buffer->capacity - 1 - length(buffer);
}

int allocBuffer(struct Buffer *buffer, int size)
{
    if (size < 0)
        return -1;
    if (availableBuffer(buffer) < size)
        return -1;
    buffer->end = (buffer->end + size) % buffer->capacity;
    return 0;
}

int freeBuffer(struct Buffer *buffer, int size)
{
    if (size < 0)
        return -1;
    if (length(buffer) < size)
        return -1;
    buffer->begin = (buffer->begin + size) % buffer->capacity;
    return 0;
}

char *bufferByte(struct Buffer *buffer, int index)
{
    return &(buffer->data[index % buffer->capacity]);
}

static inline int sendBytes(struct Buffer *buffer, int count, char bytes[])
{
    if (count >= buffer->capacity)
        return -1;
    sem_wait(buffer->mutex);
    int base = buffer->end;
    while (allocBuffer(buffer, count) == -1)
        ;
    for (int i = 0; i < count; i++)
    {
        *bufferByte(buffer, base + i) = bytes[i];
    }
    sem_post(buffer->mutex);
    return 0;
}

static inline int readBytes(struct Buffer *buffer, int count, uint8_t bytes[])
{
    sem_wait(buffer->mutex);
    int base = buffer->end;
    while (availableBuffer(buffer) < count)
        ;
    for (int i = 0; i < count; i++)
    {
        bytes[i] = *bufferByte(buffer, base + i);
    }
    freeBuffer(buffer, count);
    sem_post(buffer->mutex);
    return 0;
}
