#define _GNU_SOURCE

#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <time.h>

sem_t *items;
sem_t *freeSpace;

pid_t *consumes = NULL;
int consumeCount = 0;

pid_t *produces = NULL;
int produceCount = 0;

bool run = true;

void usr1() { run = false; }

struct Message
{
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    char data[];
};

#define MESSAGE_MAX_SIZE (sizeof(struct Message) + 255)

// int dataSize(uint8_t messageSize) { return (messageSize + 3) / 4 * 4; }

// int messageSizeForSize(uint8_t size) { return sizeof(struct Message); }

// int sendMessage(struct Buffer *buffer, struct Message *message) { return sendBytes(buffer, messageSizeForSize(message->size), message); }

// int readMessage(struct Buffer *buffer, struct Message *message) { return readBytes(buffer, messageSizeForSize(message->size), message); }q

int MessageSize(struct Message *message) { return sizeof(*message) + message->size; }

struct Message *readMessage(struct Buffer *buffer)
{
    struct Message *head = alloca(MESSAGE_MAX_SIZE);
    readBytes(buffer, sizeof(struct Message), (char *)head);
    readBytes(buffer, head->size, head->data);
    buffer->extracted++;
    printf("Extracted : %d\n", (buffer->extracted));
    return memcpy(malloc(MessageSize(head)), head, MessageSize(head));
}

uint16_t xor (int length, char bytes[]) {
    uint16_t res = 0;
    for (int i = 0; i < length; i++)
    {
        res ^= bytes[i];
    }
    return res;
}

    struct Message *randomMessage()
{
    uint8_t size = rand() % 256;
    struct Message *message = malloc(sizeof(struct Message) + size);
    *message = (struct Message){
        .type = rand(),
        .hash = 0,
        .size = size,
    };
    for (int i = 0; i < size; i++)
        message->data[i] = rand();
    message->hash = xor(sizeof(struct Message) + size, (char *)message);
    return message;
}

void freeMessage(struct Message *message) { free(message); }

void produceProc(struct Buffer *buffer)
{
    while (run)
    {
        sem_wait(freeSpace);
        struct Message *message = randomMessage();
        sendBytes(buffer, MessageSize(message), (char *)message);
        buffer->added++;
        printf("Added : %d\n", buffer->added);
        printf(
            "Producer %5d Sent message with type %02hX and hash %04hX Buffer length %d\n",
            getpid(),
            message->type,
            message->hash,
            length(buffer));
        free(message);
        sem_post(items);
        sleep(3);
    }
}

void consumeProc(struct Buffer *buffer)
{
    while (run)
    {
        sem_wait(items);
        struct Message *message = readMessage(buffer);
        printf(
            "Consumer %5d Got  message with type %02hX and hash %04hX Buffer length %d\n",
            getpid(),
            message->type,
            message->hash,
            length(buffer));
        free(message);

        sem_post(freeSpace);
        sleep(3);
    }
}

void newProduce(struct Buffer *buffer)
{
    pid_t produce = fork();
    if (produce == 0)
    {
        struct sigaction sa1 = {
            .sa_flags = 0,
            .sa_handler = usr1};
        produceProc(buffer);
    }
    printf("Fork new produce: %d\n", produce);
    produceCount++;
    produces = realloc(produces, sizeof(pid_t) * produceCount);
    produces[produceCount - 1] = produce;
}

void killProduce()
{
    if (produceCount == 0)
        return;
    produceCount--;
    printf("Kill Produce PID: %d\n", produces[produceCount]);
    kill(produces[produceCount], SIGUSR1);
    waitpid(produces[produceCount], NULL, 0);
}

void killAllProduce()
{
    while (produceCount)
        killProduce();
    free(produces);
    produces = NULL;
}

void newConsume(struct Buffer *buffer)
{
    pid_t consume = fork();
    if (consume == 0)
    {
        struct sigaction sa1 = {
            .sa_flags = 0,
            .sa_handler = usr1};
        consumeProc(buffer);
    }
    printf("Fork new produce: %d\n", consume);
    consumeCount++;
    consumes = realloc(consumes, sizeof(pid_t) * consumeCount);
    consumes[consumeCount - 1] = consume;
}

void killConsume()
{
    if (consumeCount == 0)
        return;
    consumeCount--;
    printf("Kill Consume PID: %d\n", consumes[consumeCount]);
    kill(consumes[consumeCount], SIGUSR1);
    waitpid(consumes[consumeCount], NULL, 0);
}

void killAllConsumel()
{
    while (consumeCount)
        killConsume();
    free(consumes);
    consumes = NULL;
}

int main()
{
    sem_unlink("/items");
    sem_unlink("/free_space");
    items = sem_open("/items", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR, 0);
    freeSpace = sem_open("/free_space", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR, 5);
    int capacity = 1024;
    struct Buffer *buffer = createBuffer(smalloc(sizeof(struct Buffer) + capacity), capacity);
    char opt[256];
    printf("Statr program\n");
    while (scanf("%s", opt))
    {
        if (!strcmp(opt, "p"))
            newProduce(buffer);
        if (!strcmp(opt, "kp"))
            killProduce();
        if (!strcmp(opt, "kap"))
            killAllProduce();
        if (!strcmp(opt, "c"))
            newConsume(buffer);
        if (!strcmp(opt, "kc"))
            killConsume();
        if (!strcmp(opt, "kac"))
            killAllConsumel();
        if (!strcmp(opt, "ka"))
        {
            killAllProduce();
            killAllConsumel();
        }
        if (!strcmp(opt, "q"))
            break;
    }

    sem_unlink("/items");
    sem_unlink("/free_space");
    killAllProduce();
    killAllConsumel();
    freeDesctruct(buffer);
    return 0;
}