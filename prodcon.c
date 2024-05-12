#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <unistd.h>

#define BUFFER_SIZE 10
#define BUFFER_ITEM_SIZE 30

typedef struct buffer_item {
    uint8_t data[BUFFER_ITEM_SIZE];
    uint16_t cksum;
} BUFFER_ITEM;

BUFFER_ITEM buffer[BUFFER_SIZE];
sem_t empty, full;
pthread_mutex_t mutex;
int in = 0, out = 0;

void insert_item(BUFFER_ITEM item) {
    sem_wait(&empty);
    pthread_mutex_lock(&mutex);

    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;

    pthread_mutex_unlock(&mutex);
    sem_post(&full);
}

BUFFER_ITEM remove_item() {
    BUFFER_ITEM item;
    sem_wait(&full);
    pthread_mutex_lock(&mutex);

    item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;

    pthread_mutex_unlock(&mutex);
    sem_post(&empty);
    return item;
}

uint16_t checksum(char *addr, uint32_t count) {
    register uint32_t sum = 0;
    uint16_t *buf = (uint16_t *)addr;
    while (count > 1) {
        sum += *buf++;
        count -= 2;
    }
    if (count > 0)
        sum += *(uint8_t *)buf;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

void *producer(void *arg) {
    while (1) {
        BUFFER_ITEM item;
        for (int i = 0; i < BUFFER_ITEM_SIZE; i++) {
            item.data[i] = rand() % 256;
        }
        item.cksum = checksum((char *)item.data, sizeof(item.data));
        insert_item(item);
        usleep(100000); // Sleep for 0.1 seconds
    }
    return NULL;
}

void *consumer(void *arg) {
    while (1) {
        BUFFER_ITEM item = remove_item();
        uint16_t calculated_cksum = checksum((char *)item.data, sizeof(item.data));
        if (calculated_cksum != item.cksum) {
            fprintf(stderr, "Error: Checksum mismatch. Expected: %u, Calculated: %u\n", item.cksum, calculated_cksum);
            exit(1);
        }
        usleep(100000); // Sleep for 0.1 seconds
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <delay> <#producer threads> <#consumer threads>\n", argv[0]);
        return 1;
    }

    int delay = atoi(argv[1]);
    int num_producers = atoi(argv[2]);
    int num_consumers = atoi(argv[3]);

    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full, 0, 0);
    pthread_mutex_init(&mutex, NULL);

    pthread_t producer_threads[num_producers];
    pthread_t consumer_threads[num_consumers];

    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producer_threads[i], NULL, producer, NULL);
    }

    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumer_threads[i], NULL, consumer, NULL);
    }

    sleep(delay);

    for (int i = 0; i < num_producers; i++) {
        pthread_cancel(producer_threads[i]);
    }

    for (int i = 0; i < num_consumers; i++) {
        pthread_cancel(consumer_threads[i]);
    }

    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&mutex);

    return 0;
}