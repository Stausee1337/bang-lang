#ifndef  SB_H_
#define SB_H_

#include <stdlib.h>

#include "dynarray.h"
#include "sv.h"

#define sb_append_cstr(sb, cstr)  \
    do {                          \
        const char *s = (cstr);   \
        size_t n = strlen(s);     \
        da_append_many(sb, s, n); \
    } while (0)

typedef struct {
    char* items;
    size_t count;
    size_t capacity;
} String_Builder;

String_View sb_to_string_view(String_Builder *sb);

#endif

