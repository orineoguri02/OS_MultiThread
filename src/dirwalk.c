/*
 * 디렉토리 재귀 탐색 (producer — main thread에서 호출)
 */
#include "dirwalk.h"

#include "buffer.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *join_path(const char *directory, const char *name);

int dirwalk(const char *directory, bounded_buffer_t *buffer)
{
    DIR *dir;
    struct dirent *entry;
    char *full_path;
    struct stat st;

    dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "opendir failed on %s: %s\n", directory, strerror(errno));
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        full_path = join_path(directory, entry->d_name);
        if (full_path == NULL) {
            closedir(dir);
            return -1;
        }

        if (stat(full_path, &st) != 0) {
            free(full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /* 하위 디렉토리는 재귀 탐색 */
            if (dirwalk(full_path, buffer) != 0) {
                free(full_path);
                closedir(dir);
                return -1;
            }
            free(full_path);
        } else if (S_ISREG(st.st_mode)) {
            /* 일반 파일 경로를 bounded buffer에 넣는다 (소유권 이전) */
            if (buffer_push(buffer, full_path) != 0) {
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

static char *join_path(const char *directory, const char *name)
{
    size_t dir_len;
    size_t name_len;
    int need_slash;
    size_t total;
    char *path;

    dir_len = strlen(directory);
    name_len = strlen(name);
    need_slash = (dir_len > 0 && directory[dir_len - 1] != '/');
    total = dir_len + (size_t)need_slash + name_len + 1;

    path = malloc(total);
    if (path == NULL) {
        return NULL;
    }

    if (need_slash) {
        snprintf(path, total, "%s/%s", directory, name);
    } else {
        snprintf(path, total, "%s%s", directory, name);
    }

    return path;
}
