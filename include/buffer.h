#ifndef BUFFER_H
#define BUFFER_H

#include "mtws_types.h"

int buffer_init(bounded_buffer_t *buffer, int capacity);
void buffer_destroy(bounded_buffer_t *buffer);
int buffer_push(bounded_buffer_t *buffer, char *path);
char *buffer_pop(bounded_buffer_t *buffer);
void buffer_close(bounded_buffer_t *buffer);

#endif /* BUFFER_H */
