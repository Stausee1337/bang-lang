#ifndef SV_H_
#define  SV_H_

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int) (sv).count, (sv).data

#define SV_AT(sv, idx) \
    ({ assert((idx) < (sv)->count && "Access string view out of bounds"); (sv)->data[(idx)]; })

#define sv_eq_cstr(sv, cstr) \
    sv_eq((sv), sv_from_cstring(cstr, strlen(cstr)))

#define sv_for_each(sv, name, body) \
    char name; \
    for (size_t i = 0; i < (sv)->count; i++) \
    { name = (sv)->data[i]; body }

typedef struct {
    size_t count;
    const char *data;
} String_View;

String_View sv_from_cstring(const char *data, size_t count);
bool sv_eq(String_View a, String_View b);
bool sv_startswith(String_View sv, char chr);
String_View sv_slice(String_View sv, size_t start, size_t end);

#endif // SV_H_

