#ifndef UTF8_H_
#define UTF8_H_

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dynarray.h"

typedef uint32_t uchar;

#define INVALID_CHAR    (uchar)0xffffffff

typedef struct {
    const char *data;
    size_t length;
} Utf8Slice;

typedef struct {
    DynArray data;
} Utf8String;

typedef enum {
    Error_Success,
    Error_Empty,
    Error_InvalidChar
} UTF8Error;

static inline
size_t utf8_char_size(const char utf8_char) {
#define CHR_AND(b)  ((utf8_char) & b) 
    if (CHR_AND(0x80) == 0) {
        return 1;
    }
    if (CHR_AND(0xe0) == 0xc0) {
        return 2;
    }
    if (CHR_AND(0xf0) == 0xe0) {
        return 3;
    }
    if (CHR_AND(0xf8) == 0xf0) {
        return 4;
    }
    return 0;
#undef CHR_AND
}

static inline
bool utf8_slice_get_ascii_char(Utf8Slice *slice, char *out) {
    if ((*slice->data & 0x80) == 0) {
        *out = *slice->data;
        return true;
    }
    return false;
}

static inline
bool utf8_slice_bump(Utf8Slice *string) {
    bool valid = true;
    if (string->length > 0) { 
        size_t size = utf8_char_size(*string->data);
        if (size == 0) {
            size = 1;
            valid = false;
        }
        string->data += size;
        string->length--;
    }
    return valid;
}

size_t utf8_str_getlen(const char *str, bool *contains_invalid);
uint32_t utf8_slice_get_char(Utf8Slice *slice, UTF8Error *error);
bool utf8_slice_new(const char *str, Utf8Slice *out);
Utf8Slice utf8_slice_new_unchecked(const char *str);
bool utf8_slice_slice(Utf8Slice *slice, size_t start, size_t end);
Utf8Slice utf8_slice_clone(Utf8Slice *slice);
uchar utf8_slice_get_last_char(Utf8Slice *slice, UTF8Error *error);

#endif // UTF8_H
