/*
 * Bounded buffer (producer-consumer circular queue)
 */
#include "buffer.h"

#include <pthread.h>
#include <stdlib.h>

int buffer_init(bounded_buffer_t *buffer, int capacity)
{
    buffer->items = calloc((size_t)capacity, sizeof(char *));
    if (buffer->items == NULL) {
        return -1;
    }

    /* circular queue 초기 상태 */
    buffer->capacity = capacity;
    buffer->front = 0;
    buffer->rear = 0;
    buffer->count = 0;
    buffer->closed = 0;

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->items);
        buffer->items = NULL;
        return -1;
    }
    if (pthread_cond_init(&buffer->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->items);
        buffer->items = NULL;
        return -1;
    }
    if (pthread_cond_init(&buffer->not_full, NULL) != 0) {
        pthread_cond_destroy(&buffer->not_empty);
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->items);
        buffer->items = NULL;
        return -1;
    }

    return 0;
}

void buffer_destroy(bounded_buffer_t *buffer)
{
    int i;
    int idx;

    if (buffer->items == NULL) {
        return;
    }

    /* 버퍼에 남아 있는 경로 문자열 해제 */
    for (i = 0; i < buffer->count; i++) {
        idx = (buffer->front + i) % buffer->capacity;
        free(buffer->items[idx]);
        buffer->items[idx] = NULL;
    }

    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);

    free(buffer->items);
    buffer->items = NULL;
    buffer->count = 0;
}

int buffer_push(bounded_buffer_t *buffer, char *path)
{
    pthread_mutex_lock(&buffer->mutex);

    /* 가득 찬 동안 대기. spurious wakeup 대비해 while 사용. */
    while (buffer->count == buffer->capacity && !buffer->closed) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    if (buffer->closed) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    buffer->items[buffer->rear] = path;
    buffer->rear = (buffer->rear + 1) % buffer->capacity;
    buffer->count++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

char *buffer_pop(bounded_buffer_t *buffer)
{
    char *path;

    pthread_mutex_lock(&buffer->mutex);

    /* 비어 있고 producer가 아직 끝나지 않았으면 대기 */
    while (buffer->count == 0 && !buffer->closed) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    /* producer 종료 후 버퍼가 비었으면 더 처리할 파일 없음 */
    if (buffer->count == 0 && buffer->closed) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }

    path = buffer->items[buffer->front];
    buffer->items[buffer->front] = NULL;
    buffer->front = (buffer->front + 1) % buffer->capacity;
    buffer->count--;

    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    return path;
}

void buffer_close(bounded_buffer_t *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = 1;
    /* 대기 중인 worker/producer 모두 깨운다 */
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
}
