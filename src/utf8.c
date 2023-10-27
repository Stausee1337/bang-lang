#include "utf8.h"

size_t utf8_str_getlen(const char *str, bool *contains_invalid) {
    *contains_invalid = false;

    size_t length = 0;
    for (; *str != '\0'; length++) {
        size_t char_size = utf8_char_size(*str);
        str += char_size;
        if (char_size == 0) {
            *contains_invalid = true;
            str++;
        }
    }
    return length;
}

bool utf8_slice_new(const char *str, Utf8Slice *out) {
    bool contains_invalid;
    size_t length = utf8_str_getlen(str, &contains_invalid);
    out->data = str,
    out->length = length; 
    return !contains_invalid;
}

Utf8Slice utf8_slice_new_unchecked(const char *str) {
    bool ignored;
    Utf8Slice slice = {
        .data = str,
        .length = utf8_str_getlen(str, &ignored)
    };
    return slice;
}

uint32_t utf8_slice_get_char(Utf8Slice *slice, UTF8Error *error) {
    *error = Error_Success;
    if (slice->length == 0) {
        *error = Error_Empty;
        return 0;
    }
    switch (utf8_char_size(*slice->data)) {
        case 0:
            *error = Error_InvalidChar;
        case 1:
            return (uint32_t)*slice->data;
        case 2:
            {
                char c1 = slice->data[0];
                char c2 = slice->data[1];

                return ((c1 & 0x1f) << 6) | (c2 & 0x3f);
            }
        case 3:
            {
                char c1 = slice->data[0];
                char c2 = slice->data[1];
                char c3 = slice->data[2];

                return ((c1 & 0x0f) << 0x0c) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
            }
        case 4:
            {
                char c1 = slice->data[0];
                char c2 = slice->data[1];
                char c3 = slice->data[2];
                char c4 = slice->data[3];

                return ((c1 & 0x07) << 0x12) | ((c2 & 0x3f) << 0x0c) | ((c3 & 0x3f) << 0x06) | (c4 & 0x3f);
            }
        default:    
            assert(0 && "unreachable");
            break;
    }
    return true;
}

Utf8Slice utf8_slice_clone(Utf8Slice *slice) {
    Utf8Slice rv = {
        .length = slice->length,
        .data = slice->data
    };
    return rv;
}

bool utf8_slice_slice(Utf8Slice *slice, size_t start, size_t end) {
    if (start > end ||
            end > slice->length || 
            start > slice->length) {
        return false;
    }

    slice->data += start;
    if (start == end) {
        slice->length = 0;
        return true;
    }

    slice->length = (end - start);
    slice->data += start;
    return true;
}

inline
uchar utf8_slice_get_last_char(Utf8Slice *slice, UTF8Error *error) {
    Utf8Slice clone = utf8_slice_clone(slice);
    while (clone.length > 1) {
        utf8_slice_bump(&clone);
    }
    assert(clone.length == 1 && "Slice needs to be length one");

    return utf8_slice_get_char(&clone, error);
}
