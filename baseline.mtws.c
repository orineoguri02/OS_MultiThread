#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char **buffer;
    int max;
    int use_pos;
    int fill_pos;
    int count;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t fill_cond;
    pthread_cond_t empty_cond;
} bounded_buffer_t;

typedef struct {
    int thread_index;
    const char *word;
    bounded_buffer_t *buffer;
    long long local_found;
    long long local_files;
} worker_arg_t;


static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s -b <buffer size> -t <num threads> -d <directory> -w <word>\n",
            program_name);
}

static int parse_positive_int(const char *text, const char *option_name)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 || value > 1000000) {
        fprintf(stderr, "Invalid positive integer for %s: %s\n", option_name, text);
        exit(1);
    }

    return (int)value;
}

static void lowercase_inplace(char *text)
{
    int i = 0;

    while (text[i] != '\0') {
        if (text[i] >= 'A' && text[i] <= 'Z') {
            text[i] = text[i] + 32;
        }
        i++;
    }
}

static int parse_args(int argc, char *argv[],
                      int *buffer_size, int *thread_count,
                      const char **directory, char **word)
{
    int opt;
    struct stat st;

    *buffer_size = -1;
    *thread_count = -1;
    *directory = NULL;
    *word = NULL;

    while ((opt = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (opt) {
        case 'b':
            *buffer_size = parse_positive_int(optarg, "-b");
            break;
        case 't':
            *thread_count = parse_positive_int(optarg, "-t");
            break;
        case 'd':
            *directory = optarg;
            break;
        case 'w':
            *word = strdup(optarg);
            if (*word == NULL) {
                perror("strdup");
                goto fail;
            }
            lowercase_inplace(*word);
            break;
        default:
            print_usage(argv[0]);
            goto fail;
        }
    }

    if (*buffer_size <= 0 || *thread_count <= 0 || *directory == NULL || *word == NULL) {
        print_usage(argv[0]);
        goto fail;
    }

    if ((*word)[0] == '\0') {
        fprintf(stderr, "Error: search word must not be empty.\n");
        goto fail;
    }

    if (stat(*directory, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: directory does not exist or is not a directory: %s\n",
                *directory);
        goto fail;
    }

    return 0;

fail:
    free(*word);
    *word = NULL;
    return 1;
}

/* --- bounded buffer --- */

static int buffer_init(bounded_buffer_t *buffer, int max)
{
    buffer->buffer = calloc((size_t)max, sizeof(char *)); // 버퍼 초기화
    if (buffer->buffer == NULL) {
        return -1;
    }

    buffer->max = max;
    buffer->use_pos = 0;
    buffer->fill_pos = 0;
    buffer->count = 0;
    buffer->closed = 0;

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        goto fail_items;
    }
    if (pthread_cond_init(&buffer->fill_cond, NULL) != 0) {
        goto fail_mutex;
    }
    if (pthread_cond_init(&buffer->empty_cond, NULL) != 0) {
        goto fail_fill_cond;
    }

    return 0;

fail_fill_cond:
    pthread_cond_destroy(&buffer->fill_cond);
fail_mutex:
    pthread_mutex_destroy(&buffer->mutex);
fail_items:
    free(buffer->buffer);
    buffer->buffer = NULL;
    return -1;
}

static void buffer_destroy(bounded_buffer_t *buffer) 
{
    if (buffer->buffer == NULL) {
        return;
    }

    for (int i = 0; i < buffer->count; i++) {
        int idx = (buffer->use_pos + i) % buffer->max;
        free(buffer->buffer[idx]);
    }

    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->fill_cond);
    pthread_cond_destroy(&buffer->empty_cond);

    free(buffer->buffer);
    buffer->buffer = NULL;
    buffer->count = 0;
}

static int put(bounded_buffer_t *buffer, char *path)
{
    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == buffer->max && !buffer->closed) {
        pthread_cond_wait(&buffer->empty_cond, &buffer->mutex);
    }

    if (buffer->closed) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    buffer->buffer[buffer->fill_pos] = path;
    buffer->fill_pos = (buffer->fill_pos + 1) % buffer->max;
    buffer->count++;

    pthread_cond_signal(&buffer->fill_cond);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

static char *get(bounded_buffer_t *buffer)
{
    char *path;

    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == 0 && !buffer->closed) {
        pthread_cond_wait(&buffer->fill_cond, &buffer->mutex);
    }

    if (buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }

    path = buffer->buffer[buffer->use_pos];
    buffer->buffer[buffer->use_pos] = NULL;
    buffer->use_pos = (buffer->use_pos + 1) % buffer->max;
    buffer->count--;

    pthread_cond_signal(&buffer->empty_cond);
    pthread_mutex_unlock(&buffer->mutex);
    return path;
}

static void buffer_close(bounded_buffer_t *buffer)
{
    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = 1;
    pthread_cond_broadcast(&buffer->fill_cond);
    pthread_cond_broadcast(&buffer->empty_cond);
    pthread_mutex_unlock(&buffer->mutex);
}

/* --- 디렉토리 탐색 (producer) --- */

static char *join_path(const char *directory, const char *name)
{
    size_t len = strlen(directory) + strlen(name) + 2;
    char *path = malloc(len);

    if (path == NULL) {
        return NULL;
    }

    snprintf(path, len, "%s/%s", directory, name);
    return path;
}

static int dirwalk(const char *directory, bounded_buffer_t *buffer)
{
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "opendir failed on %s: %s\n", directory, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *full_path = join_path(directory, entry->d_name);
        if (full_path == NULL) {
            closedir(dir);
            return -1;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            free(full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (dirwalk(full_path, buffer) != 0) {
                free(full_path);
                closedir(dir);
                return -1;
            }
            free(full_path);
        } else if (S_ISREG(st.st_mode)) {
            if (put(buffer, full_path) != 0) {
                free(full_path);
                closedir(dir);
                return -1;
            }
        } else {
            free(full_path);
        }
    }

    closedir(dir);
    return 0;
}

/* --- 파일 검색 --- */

static long long count_occurrences(const char *text, const char *word)
{
    size_t text_len = strlen(text);
    size_t word_len = strlen(word);
    long long count = 0;
    size_t i, j;
    int matched;

    if (word_len == 0 || text_len < word_len) {
        return 0;
    }

    for (i = 0; i <= text_len - word_len; ) {
        matched = 1;

        for (j = 0; j < word_len; j++) {
            if (text[i + j] != word[j]) {
                matched = 0;
                break;
            }
        }

        if (matched) {
            count++;
            i += word_len;
        } else {
            i++;
        }
    }

    return count;
}

static long long search_count_in_file(const char *path, const char *word)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t nread;
    long long total = 0;

    while ((nread = getline(&line, &line_cap, fp)) != -1) {
        if (nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }
        lowercase_inplace(line);
        total += count_occurrences(line, word);
    }

    free(line);
    fclose(fp);
    return total;
}

/* --- worker --- */

static void *worker_main(void *arg)
{
    worker_arg_t *worker = (worker_arg_t *)arg;
    long long file_seq = 0;
    long long local_found = 0;
    long long local_files = 0;

    printf("[Thread#%d] started searching '%s'...\n",
           worker->thread_index, worker->word);

    while (1) {
        char *path;
        long long count;

        path = get(worker->buffer);

        if (path == NULL) {
            break;
        }

        count = search_count_in_file(path, worker->word);

        printf("[Thread#%d-%lld] %s : %lld found\n",
               worker->thread_index, file_seq, path, count);

        /* 성능 개선: 파일 검색 결과를 로컬 변수에 저장하여 공유 뮤텍스 잠금을 최소화*/
        local_found = local_found + count;
        local_files = local_files + 1;
        file_seq = file_seq + 1;

        free(path);
    }

    worker->local_found = local_found;
    worker->local_files = local_files;
    return NULL;
}

/* --- main --- */

int main(int argc, char *argv[])
{
    int buffer_size;
    int thread_count;
    const char *directory;
    char *word = NULL;

    if (parse_args(argc, argv, &buffer_size, &thread_count, &directory, &word) != 0) {
        return 1;
    }

    bounded_buffer_t buffer;
    if (buffer_init(&buffer, buffer_size) != 0) {
        fprintf(stderr, "Failed to initialize bounded buffer.\n");
        free(word);
        return 1;
    }

    printf("Buffer size=%d, Num threads=%d, Directory=%s, SearchWord=%s\n",
           buffer_size, thread_count, directory, word);

    pthread_t *threads = calloc((size_t)thread_count, sizeof(pthread_t));
    worker_arg_t *worker_args = calloc((size_t)thread_count, sizeof(worker_arg_t));
    if (threads == NULL || worker_args == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        buffer_destroy(&buffer);
        free(threads);
        free(worker_args);
        free(word);
        return 1;
    }

    for (int i = 0; i < thread_count; i++) {
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

    if (dirwalk(directory, &buffer) != 0) {
        fprintf(stderr, "Directory walk failed.\n");
    }
    
    buffer_close(&buffer);
    
    long long total_found = 0;
    long long total_files = 0;
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        total_found = total_found + worker_args[i].local_found;
        total_files = total_files + worker_args[i].local_files;
    }
    
    printf("Total found = %lld (Num files=%lld)\n", total_found, total_files);
    
    buffer_destroy(&buffer);
    free(threads);
    free(worker_args);
    free(word);
    return 0;
}
