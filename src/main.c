#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "dynarray.h"

int main() {
    DynArray_Result result = dynarray_create(uint32_t);
    if (result.error != Created) {
        assert(false && "Array couldn't be created");
    }
    DynArray list = result.data.array;
    printf("Count: %zu; Cap: %zu\n", list.dyn_count, list.dyn_capacity);
    uint32_t size = 42;
    dynarray_push(&list, size);
    printf("Count: %zu; Cap: %zu\n", list.dyn_count, list.dyn_capacity);

    uint32_t* item = dynarray_get_at(uint32_t, &list, 0);
    printf("Item: %u\n", *item);

    dynarray_set_at(&list, 0, 1337);

    item = dynarray_get_at(uint32_t, &list, 0);
    printf("Item: %u\n", *item);

    dynarray_free(&list);
    return 0;
    FILE *stream = fopen("test.txt", "r");

    fseek(stream, 0L, SEEK_END);
    size_t fsize = ftell(stream);

    char *data = malloc(fsize + 1);
    memset(data, 0, fsize + 1);

    fseek(stream, 0L, SEEK_SET);
    fread(data, 1, fsize, stream);

    fclose(stream);

    LexState lexer = lexer_init(data);
    lexer_lex(&lexer);

    printf("Token: %d\n", lexer.token);
    printf("Error: %d\n", lexer.error);
}
