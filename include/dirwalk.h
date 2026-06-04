#ifndef DIRWALK_H
#define DIRWALK_H

#include "mtws_types.h"

/* -d 디렉토리를 재귀 탐색하며 일반 파일 경로를 buffer에 넣는다 (producer). */
int dirwalk(const char *directory, bounded_buffer_t *buffer);

#endif /* DIRWALK_H */
