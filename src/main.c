#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lexer.h"
#include "strings.h"
#include "parser.h"

void print_token_tree(String_Builder *sb, Lex_TokenTree tree) {
    switch (tree.type) {
        case Tt_Token:
            lexer_print_token(sb, &tree.Tt_Token);
            break;
        case Tt_Delimited:
            lexer_print_delimited(sb, &tree.Tt_Delimited);
            break;
    }
    printf(SV_FMT "\n", SV_ARG(sb_to_string_view(sb)));
}

void print_token_stream(Lex_TokenStream stream, int level) {
    String_Builder sb = {0};
    for (size_t i = 0; i < stream.count; i++) {
        Lex_TokenTree tree = stream.items[i];

        sb.count = 0;
        for (int j = 0; j < level; j++) {
            sb_append_cstr(&sb, "    ");
        }
        print_token_tree(&sb, tree);

        if (tree.type == Tt_Delimited) {
            print_token_stream(tree.Tt_Delimited.stream, level+1);
        }
    }

    free(sb.items);
}

#define return_defer(value) \
    do { result = (value); goto defer; } while (0)

bool read_entire_file(const char *filename, String_Builder *sb) {
    bool result = true;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return_defer(false);
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        return_defer(false);
    }
    long fsize = ftell(file);
    if (fsize == -1) {
        return_defer(false);
    }

    if (sb->capacity - sb->count < fsize) {
        sb->items = realloc(sb->items, sb->count + fsize);
        sb->capacity = sb->count + fsize;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        return_defer(false);
    }
    if (fread(sb->items + sb->count, 1, fsize, file) == 0 && fsize > 0) {
        return_defer(false);
    }
    sb->count += fsize;

    // if (errno)

    if (fclose(file) != 0) {
        return_defer(false);
    }

defer:
    if (!result) {
        fprintf(stderr, "ERROR: Could not read file: %s: %s\n", filename, strerror(errno));
    }
    return result;
}

const char *shift_args(char ***argv, int *argc) {
    if (*argc <= 0) {
        assert(false && "argv is empty");
    }
    const char *result = (*argv)[0];
    (*argv)++;
    (*argc)--;
    return result;
}

int main(int argc, char **argv) {
    const char* program = shift_args(&argv, &argc);
    if (argc <= 0) {
        fprintf(stderr, "Usage: %s <source file>\n", program);
        fprintf(stderr, "ERROR: No input files\n");
        return 1;
    }

    const char* filename = shift_args(&argv, &argc);

    String_Builder content = {0};
    if (!read_entire_file(filename, &content)) {
        return 1;
    }

    bool success;
    Lex_TokenizeResult result = 
        lexer_tokenize_source(
            sv_from_cstring(filename, strlen(filename)), 
            sb_to_string_view(&content),
            &success
        );

    if (!success) {
        String_Builder sb = {0};
        lexer_print_error(&sb, &result.error.type);
        sb_append_cstr(&sb, " at ");
        lexer_print_span(&sb, result.error.span);

        printf(SV_FMT "\n", SV_ARG(sb_to_string_view(&sb)));
        free(sb.items);
        free(content.items);
        exit(1);
    }
    Lex_TokenStream stream = result.stream;
    // print_token_stream(stream, 0);

    Ast_Stmt* stmt = parser_parse(stream);
    String_Builder sb = {0};
    ast_print_stmt(&sb, stmt, 0);

    printf(SV_FMT"\n", SV_ARG(sb_to_string_view(&sb)));

    lexer_token_stream_free(&stream);
    free(content.items);


    return 0;
}
