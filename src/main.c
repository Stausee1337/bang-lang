#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "strings.h"

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

#include "parser.h"

int main() {
    const char *filename = "stuff/test.txt";
    FILE *file = fopen(filename, "r");

    fseek(file, 0L, SEEK_END);
    size_t fsize = ftell(file);

    char *data = malloc(fsize + 1);
    memset(data, 0, fsize + 1);

    fseek(file, 0L, SEEK_SET);
    fread(data, 1, fsize, file);

    fclose(file);

    bool success;
    Lex_TokenizeResult result = 
        lexer_tokenize_source(
            sv_from_cstring(filename, strlen(filename)), 
            sv_from_cstring(data, strlen(data)),
            &success
        );

    if (!success) {
        String_Builder sb = {0};
        lexer_print_error(&sb, &result.error.type);
        sb_append_cstr(&sb, " at ");
        lexer_print_span(&sb, result.error.span);

        printf(SV_FMT "\n", SV_ARG(sb_to_string_view(&sb)));
        free(sb.items);
        free(data);
        exit(1);
    }
    Lex_TokenStream stream = result.stream;
    // print_token_stream(stream, 0);

    Ast_Expr *expr = parser_parse(stream);
    String_Builder sb = {0};
    ast_print_expr(&sb, expr, 0);

    printf(SV_FMT"\n", SV_ARG(sb_to_string_view(&sb)));

    lexer_token_stream_free(&stream);
    free(data);


    return 0;
}
