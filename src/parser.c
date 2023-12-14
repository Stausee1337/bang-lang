#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"
#include "dynarray.h"
#include "strings.h"

#define New_Impl(type, expr) \
    ({ type __value = (expr); heap_alloc(&__value, sizeof(__value)); })
#define New(expr) (typeof((expr))*)New_Impl(typeof((expr)), (expr))

static
void *heap_alloc(void* src, size_t size) {
    void *dest = malloc(size);
    memcpy(dest, src, size);
    return dest;
}

typedef struct {
    Lex_TokenStream stream;
    size_t item;
} TokenTreeCursor;

typedef struct {
    TokenTreeCursor cursor;
    Lex_Delimiter delimiter;
    struct {
        Lex_Span open;
        Lex_Span close;
    } span;
} TreeTriple;

typedef struct {
    TreeTriple *items;
    size_t count;
    size_t capacity;
} TreeTriples;

typedef struct {
    TokenTreeCursor tree_cursor;
    TreeTriples stack;
} TokenCursor;

typedef struct {
    Lex_Token token;
    TokenCursor cursor;
} Parser;

static inline
char _char_for_delimiter(Lex_Delimiter delimiter, bool opening) {
    switch (delimiter) {
        case Dl_Paren:
            return opening ? '(' : ')';
        case Dl_Brace:
            return opening ? '{' : '}';
        case Dl_Bracket:
            return opening ? '[' : ']';
    }
    assert(false && "unreachable");
}

static 
Lex_Token _cursor_next(TokenCursor *c) {
    size_t next_idx = c->tree_cursor.item;
    if (next_idx >= c->tree_cursor.stream.count) {
        assert(c->stack.count > 0 && "next_token(Parser*) after Tk_EOF is forbidden");
        TreeTriple triple = c->stack.items[--c->stack.count];
        c->tree_cursor = triple.cursor;
        return (Lex_Token) {
            .kind = _char_for_delimiter(triple.delimiter, /* opening */ false),
            .span = triple.span.close
        };
    }
    c->tree_cursor.item++;
    Lex_TokenTree *tree = &c->tree_cursor.stream.items[next_idx];
    if (tree->type == Tt_Delimited) {
        Lex_Delimited *delimited = &tree->Tt_Delimited;

        TokenTreeCursor current_ttc = c->tree_cursor;
        TreeTriple triple = (TreeTriple) {
            .cursor = current_ttc,
            .delimiter = delimited->delimiter,
            .span = {
                .open = delimited->span.open,
                .close = delimited->span.close,
            }
        };
        da_append(&c->stack, triple);

        c->tree_cursor = (TokenTreeCursor) { .stream = delimited->stream, .item = 0 };

        return (Lex_Token) {
            .kind = _char_for_delimiter(delimited->delimiter, /* opening */ true),
            .span = delimited->span.open
        };
    } else {
        return tree->Tt_Token;
    }
}

#define _U(v) (void)v

static
Lex_Token next_token(Parser *p) {
    return (p->token = _cursor_next(&p->cursor));    
}

static
Lex_Token* lookahead(Parser *p, size_t ll) {
    _U(p);
    _U(ll);
    assert(false && "Not implemented");
}

bool is_eof(Parser *p) {
    return p->token.kind == Tk_EOF;
}

static
Ast_Expr *parse_expression(Parser *p) {
    String_Builder sb = {0};
    while (true) {
        sb.count = 0;
        lexer_print_token(&sb, &p->token);
        printf(SV_FMT"\n", SV_ARG(sb_to_string_view(&sb)));
        next_token(p);
        if (is_eof(p)) {
            break;
        }
    }
    return NULL;
}

Ast_Expr *parser_parse(Lex_TokenStream stream) {
    Parser p = {
        .token = {
            .kind = Tk_INIT,
            .span = { .start = {0}, .end = {0} }
        },
        .cursor = {
            .tree_cursor = { .stream = stream, .item = 0 },
            .stack = {0}
        }
    };
    return parse_expression(&p);
}

