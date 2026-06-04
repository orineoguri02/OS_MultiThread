#ifndef MTWS_TYPES_H
#define MTWS_TYPES_H

#include <pthread.h>

/* Producer(main)와 worker가 공유하는 bounded buffer (circular queue). */
typedef struct {
    char **items;
    int capacity;
    int front;
    int rear;
    int count;
    int closed; /* producer 종료 신호 (체크리스트의 done과 동일 역할) */
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} bounded_buffer_t;

/* worker thread 하나당 전달하는 인자. */
typedef struct {
    int thread_index;
    const char *word; /* main에서 이미 소문자로 변환된 검색어 */
    bounded_buffer_t *buffer;
    long long local_found;
    long long local_files;
} worker_arg_t;

#endif /* MTWS_TYPES_H */
