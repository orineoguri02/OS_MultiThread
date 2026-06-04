/*
 * 파일 내 단어 검색 (case-insensitive)
 * search_word는 main/args에서 이미 소문자로 변환되어 있다.
 */
#include "search.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lowercase_inplace(char *text)
{
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}

/* 한 줄에서 검색어가 몇 번 나오는지 센다 (겹치지 않게 진행). */
static long long count_occurrences(const char *haystack, const char *needle)
{
    size_t needle_len;
    long long count = 0;
    const char *pos;

    needle_len = strlen(needle);
    if (needle_len == 0) {
        return 0;
    }

    pos = haystack;
    while ((pos = strstr(pos, needle)) != NULL) {
        count++;
        pos += needle_len;
    }

    return count;
}

long long search_count_in_file(const char *path, const char *word)
{
    FILE *fp;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t nread;
    long long total = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

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
