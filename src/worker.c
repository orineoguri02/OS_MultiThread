/*
 * Worker thread: buffer에서 경로 pop → 검색 → 결과 출력
 *
 * 성능 개선(thread-local aggregation):
 *   파일마다 global mutex를 잡지 않고 local_found/local_files에만 누적하고,
 *   main thread가 pthread_join 이후 한 번에 합산한다.
 */
#include "worker.h"

#include "buffer.h"
#include "search.h"

#include <stdio.h>
#include <stdlib.h>

void *worker_main(void *arg)
{
    worker_arg_t *worker = (worker_arg_t *)arg;
    char *path;
    long long count;
    long long file_seq = 0;

    printf("[Thread#%d] started searching '%s'...\n",
           worker->thread_index, worker->word);

    while (1) {
        path = buffer_pop(worker->buffer);
        if (path == NULL) {
            break;
        }

        count = search_count_in_file(path, worker->word);

        /* PDF 예시 형식: [Thread#스레드번호-파일순번] 경로 : 개수 found */
        printf("[Thread#%d-%lld] %s : %lld found\n",
               worker->thread_index, file_seq, path, count);

        worker->local_found += count;
        worker->local_files++;
        file_seq++;

        free(path);
    }

    return NULL;
}
