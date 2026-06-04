/*
 * HW#3 multi-threaded word search — 진입점
 *
 * 흐름: 인자 파싱 → buffer/worker 준비 → main이 producer(dirwalk) →
 *       buffer_close → worker join → 전체 합계 출력
 */
#include "args.h"
#include "buffer.h"
#include "dirwalk.h"
#include "mtws_types.h"
#include "worker.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int buffer_size;
    int thread_count;
    const char *directory;
    char *word = NULL;

    bounded_buffer_t buffer;
    pthread_t *threads = NULL;
    worker_arg_t *worker_args = NULL;
    int i;
    long long total_found = 0;
    long long total_files = 0;

    if (args_parse(argc, argv, &buffer_size, &thread_count, &directory, &word) != 0) {
        return 1;
    }

    if (buffer_init(&buffer, buffer_size) != 0) {
        fprintf(stderr, "Failed to initialize bounded buffer.\n");
        free(word);
        return 1;
    }

    /* 과제 PDF와 동일한 형식의 시작 배너 */
    printf("Buffer size=%d, Num threads=%d, Directory=%s, SearchWord=%s\n",
           buffer_size, thread_count, directory, word);

    threads = calloc((size_t)thread_count, sizeof(pthread_t));
    worker_args = calloc((size_t)thread_count, sizeof(worker_arg_t));
    if (threads == NULL || worker_args == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        buffer_destroy(&buffer);
        free(threads);
        free(worker_args);
        free(word);
        return 1;
    }

    /* worker thread 생성 (-t 개수, main thread 제외) */
    for (i = 0; i < thread_count; i++) {
        worker_args[i].thread_index = i;
        worker_args[i].word = word;
        worker_args[i].buffer = &buffer;
        worker_args[i].local_found = 0;
        worker_args[i].local_files = 0;

        if (pthread_create(&threads[i], NULL, worker_main, &worker_args[i]) != 0) {
            fprintf(stderr, "pthread_create failed for worker %d\n", i);
            buffer_close(&buffer);
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            buffer_destroy(&buffer);
            free(threads);
            free(worker_args);
            free(word);
            return 1;
        }
    }

    /* main thread가 producer: 디렉토리 재귀 탐색 후 경로를 buffer에 push */
    if (dirwalk(directory, &buffer) != 0) {
        fprintf(stderr, "Directory walk failed.\n");
    }

    /* 탐색 종료 신호 — worker들이 빈 버퍼에서 깨어나 종료할 수 있게 함 */
    buffer_close(&buffer);

    /* worker 종료 대기 후 thread-local 결과를 main에서 합산 (성능 개선 방식) */
    for (i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        total_found += worker_args[i].local_found;
        total_files += worker_args[i].local_files;
    }

    printf("Total found = %lld (Num files=%lld)\n", total_found, total_files);

    buffer_destroy(&buffer);
    free(threads);
    free(worker_args);
    free(word);
    return 0;
}
