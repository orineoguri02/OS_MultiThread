#ifndef ARGS_H
#define ARGS_H

/* 파싱 성공 시 0, usage/오류 시 0이 아닌 값. word는 strdup된 힙 메모리. */
int args_parse(int argc, char *argv[],
               int *buffer_size, int *thread_count,
               const char **directory, char **word);

void args_usage(const char *program_name);

#endif /* ARGS_H */
