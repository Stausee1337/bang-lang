
#include "sb.h"

String_View sb_to_string_view(String_Builder *sb) {
    String_View sv;
    sv.data = sb->items;
    sv.count = sb->count;
    return sv;
}
