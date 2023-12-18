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
void next_token(Parser *p) {
    p->token = _cursor_next(&p->cursor);
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
Lex_Token expect(Parser *p, Lex_TokenKind kind) {
    assert(p->token.kind == kind && "Expected something else");
    Lex_Token ret = p->token;
    next_token(p);
    return ret;
}

static
Ast_Expr *parse_expression(Parser *p, int min_prec);

Ast_Expr *parse_path(Parser *p) {
    Ast_Path path = {0};
    Lex_Span span = (Lex_Span) {
        .start = p->token.span.start,
        .end = {0},
        .filename = p->token.span.filename
    };

    while (true) {
        Lex_Token ident = expect(p, Tk_Ident);
        // TODO: Support Generic Argument Parsing
        Ast_PathSegment segment = {
            .ident = ident.Tk_Ident.name
        };
        da_append(&path, segment);
        if (p->token.kind != ':') {
            span.end = ident.span.end;
            break;
        }
        next_token(p);
    }

    return New(create_expr(Path)(span, { .path = path }));
}

#define return_defer(...) \
    do { return_value = __VA_ARGS__; goto defer; } while (0)

static
Ast_Expr *parse_primary(Parser *p) {
    Lex_Token token = p->token;
    Ast_Expr *return_value = NULL;

    // TODO: with `lookahead()` check :EnumMember patterns
    switch ((int)token.kind) {
        case Tk_Char:
            return_defer(New(create_expr(Literal)(token.span, {
                .kind = L_Char,
                .wchar = token.Tk_Char.wchar,
            })));
        case Tk_String:
            // TODO: unescape string
            return_defer(New(create_expr(Literal)(token.span, {
                .kind = L_String,
                .string = token.Tk_String.string,
            })));
        case Tk_Number: {
            if (IS_FLOAT_CLASS(token.Tk_Number.nclass)) {
                return_defer(New(create_expr(Literal)(token.span, {
                    .kind = L_Float,
                    .floating = token.Tk_Number.number.floating,
                    .nclass = token.Tk_Number.nclass
                })));
            }
            return_defer(New(create_expr(Literal)(token.span, {
                .kind = L_Integer,
                .integer = token.Tk_Number.number.integer,
                .nclass = token.Tk_Number.nclass
            })));
        }
        case Tk_Keyword: {
            Lex_Keyword keyword = token.Tk_Keyword.keyword;
            switch (keyword) {
                case K_True:
                case K_False:
                    return_defer(New(create_expr(Literal)(token.span, {
                        .kind = L_Boolean,
                        .boolean = keyword == K_True
                    })));
                case K_Nil:
                    return_defer(New(create_expr(Literal)(token.span, {
                        .kind = L_Nil,
                    })));
                default: break;
            }
        } break;
        case '(': {
            // TODO: parsing tuples and arrow functions () -> something
            next_token(p);
            Ast_Expr *expr = parse_expression(p, 0);
            Lex_Token endtoken = expect(p, ')');
            Lex_Span span = {
                .start = token.span.start,
                .end = endtoken.span.end,
                .filename = token.span.filename
            };
            return New(create_expr(Paren)(span, { .expr = expr }));
        } break;
        case Tk_Ident:
            return parse_path(p);
        default: break;
    }
    assert(false && "SyntaxError: expected string, char, number, boolean, nil or identifier");
defer: 
    next_token(p);
    return return_value;
}

static
Ast_Expr *parse_call(Parser *p, Ast_Expr *base) {
    expect(p, (Lex_TokenKind)'(');

    Ast_Exprs arguments = {0};
    if (p->token.kind == ')') {
        goto end;
    }
    while (true) {
        Ast_Expr *arg = parse_expression(p, 0);
        da_append(&arguments, arg);
        Lex_TokenKind kind = p->token.kind;
        if (kind != ',' && kind != ')') {
            assert(false && "Expected comma or closing parenthesis");
        }
        if (kind == ',') {
            next_token(p);
        }
        if (kind == ')') {
            break;
        }
    }

end:
{
    Lex_Token endtoken = p->token;
    next_token(p); // skip )
    Lex_Span span = {
        .start = base->span.start,
        .end = endtoken.span.end,
        .filename = endtoken.span.filename
    };
    return New(create_expr(Call)(span, { .function = base, .arguments = arguments }));
}
}

static
Ast_Expr *parse_postfix(Parser *p, Ast_Expr *base, bool *matched) {
    switch ((int)p->token.kind) {
        case '(': {
            *matched = true;
            return parse_call(p, base);
        } break;
        case '[': // parse subscript
            assert(false && "Not implemented");
            break;
        case '.': {
            // TODO: Parse buitlin suffixes like `.!` or `.?`
            *matched = true;
            next_token(p);
            Lex_Token ident = expect(p, Tk_Ident);
            Lex_Span span = {
                .start = base->span.start,
                .end = ident.span.end,
                .filename = ident.span.filename
            };
            return New(create_expr(Member)(span, { .expr = base, .ident = ident.Tk_Ident.name }));
        } break;
    }
    return base;
}

typedef struct {
    enum {
        Op_Assignment,
        Op_Binary
    } kind;
    enum {
        Assoc_Right,
        Assoc_Left,
    } accociativity;
    union {
        BinaryOp Op_Binary;
        AssignmentOp Op_Assignment;
    };
    int precedence;
} AssocOp;

static_assert(Assoc_Left == 1, "Assoc_Left = 1");

static
bool is_associative_operator(Parser *p, AssocOp *op) {
    if (IS_TOKEN_KIND(p->token.kind)) {
        return false;
    }
    BinaryOp binop = binary_op_resolve(p->token.kind);
    if (binop != Bo_Invalid) {
        op->precedence = binary_op_get_precedence(binop);
        op->kind = Op_Binary;
        op->Op_Binary = binop;
        op->accociativity = Assoc_Left;
        return true;
    }
    AssignmentOp assgnop = assignment_op_resolve(p->token.kind);
    if (assgnop != Ao_Invalid) {
        op->precedence = 2;
        op->kind = Op_Assignment;
        op->Op_Assignment = assgnop;
        op->accociativity = Assoc_Right;
        return true;
    }
    return false;
}

static
Ast_Expr *parse_expr_prefix(Parser *p);

static
Ast_Expr *parse_ref(Parser *p, Lex_Pos start) {
    // TODO: parse refrence modifier `let`
    Ast_Expr *expr = parse_expr_prefix(p);
    Lex_Span span = {
        .start = start,
        .end = expr->span.end,
        .filename = expr->span.filename
    };
    return New(create_expr(Refrence)(span, { .expr = expr }));
}

static
Ast_Expr *parse_expr_prefix(Parser *p) {
    Lex_Token starttok = p->token;
    if (!IS_TOKEN_KIND(p->token.kind)) {
        UnaryOp unary = unary_op_resolve(p->token.kind);
        if (unary != Uo_Invalid) {
            next_token(p);
            Ast_Expr *expr = parse_expr_prefix(p);
            Lex_Span span = {
                .start = starttok.span.start,
                .end = expr->span.end,
                .filename = starttok.span.filename
            };
            return New(create_expr(Unary)(span, { .op = unary, .expr = expr }));
        } else if (p->token.kind == '&') {
            next_token(p);
            return parse_ref(p, starttok.span.start);
        } else if (p->token.kind == DOUBLE_AND) {
            // Don't nex_token() the parser, replace `&&` with two seperate &-s
            Lex_Span span = {
                .start =  {
                    .row = starttok.span.start.row,
                    .col = starttok.span.start.col + 1
                },
                .end = starttok.span.end,
                .filename = starttok.span.filename
            };
            p->token = (Lex_Token) {
                .kind = '&',
                .span = span
            };
            return parse_ref(p, starttok.span.start);
        }
    }
    Ast_Expr *expr = parse_primary(p);
    while (true) {
        bool matched = false;
        expr = parse_postfix(p, expr, &matched);
        if (!matched) {
            break;
        }
    }

    return expr;
}

static
Ast_Expr *parse_expression(Parser *p, int min_prec) {
    Ast_Expr *lhs = parse_expr_prefix(p);

    AssocOp op;
    while (is_associative_operator(p, &op)) {
        int prec = op.precedence;
        if (prec < min_prec) {
            break;
        }
        next_token(p);

        // TODO: detect chained comparison

        Ast_Expr *rhs = parse_expression(p, prec + op.accociativity);
        Lex_Span span = {
            .start = lhs->span.start,
            .end = rhs->span.end,
            .filename = lhs->span.filename
        };

        switch (op.kind) {
            case Op_Assignment: {
                lhs = New(create_expr(Assign)(span, { .op = op.Op_Assignment, .lhs = lhs, .rhs = rhs }));
            } break;
            case Op_Binary: {
                lhs = New(create_expr(Binary)(span, { .op = op.Op_Binary, .lhs = lhs, .rhs = rhs }));
            } break;
            default:
                assert(false && "unreachable");
        };
    }
    return lhs;
}

Ast_Expr *parser_parse(Lex_TokenStream stream) {
    Parser p = {
        .token = {
            .kind = Tk_INIT,
            .span = { .start = {0}, .end = {0}, .filename = {0} }
        },
        .cursor = {
            .tree_cursor = { .stream = stream, .item = 0 },
            .stack = {0}
        }
    };
    next_token(&p);
    return parse_expression(&p, 0);
}

