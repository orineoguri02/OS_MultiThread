/*
 * 명령행 인자: getopt로 -b -t -d -w 파싱
 */
#include "args.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int parse_positive_int(const char *text, const char *option_name);
static void lowercase_string(char *text);

void args_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s -b <buffer size> -t <num threads> -d <directory> -w <word>\n",
            program_name);
}

int args_parse(int argc, char *argv[],
               int *buffer_size, int *thread_count,
               const char **directory, char **word)
{
    int opt;
    struct stat st;

    *buffer_size = -1;
    *thread_count = -1;
    *directory = NULL;
    *word = NULL;

    /* getopt로 필수 옵션 네 개를 읽는다. */
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
                return 1;
            }
            /* 검색어는 프로그램 시작 시 한 번만 소문자로 바꿔 둔다. */
            lowercase_string(*word);
            break;
        default:
            args_usage(argv[0]);
            free(*word);
            *word = NULL;
            return 1;
        }
    }

    /* 옵션이 하나라도 빠지면 usage 출력. */
    if (*buffer_size <= 0 || *thread_count <= 0 || *directory == NULL || *word == NULL) {
        args_usage(argv[0]);
        free(*word);
        *word = NULL;
        return 1;
    }

    /* -w "" 같이 빈 검색어는 허용하지 않는다. */
    if ((*word)[0] == '\0') {
        fprintf(stderr, "Error: search word must not be empty.\n");
        free(*word);
        *word = NULL;
        return 1;
    }

    /* -d 경로가 실제 디렉토리인지 확인한다. */
    if (stat(*directory, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: directory does not exist or is not a directory: %s\n",
                *directory);
        free(*word);
        *word = NULL;
        return 1;
    }

    return 0;
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

static void lowercase_string(char *text)
{
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}
