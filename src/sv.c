#include <assert.h>

#include "sv.h"

String_View sv_from_cstring(const char *data, size_t count) {
    String_View sv;
    sv.count = count;
    sv.data = data;
    return sv;
}

String_View sv_slice(String_View sv, size_t start, size_t end) {
    if (start > end) {
        assert(false && "sv_slice: start can only be <= end");
    }
    if (start >= sv.count) {
        assert(false && "sv_slice: start is outside of the boundaries of the sv");
    }
    size_t new_count = end - start;
    if (new_count > sv.count) {
        new_count = sv.count;
    }
    String_View result;
    result.count = new_count;
    result.data = sv.data + start;
    return result;
}

bool sv_eq(String_View a, String_View b) {
    if (a.count != b.count) return false;
    return memcmp(a.data, b.data, a.count) == 0;
}

bool sv_startswith(String_View sv, char chr) {
    if (sv.count == 0) return false; 
    return SV_AT(&sv, 0) == chr;
}


