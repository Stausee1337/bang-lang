#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"
#include "dynarray.h"

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
        if (c->stack.count == 0) {
            // return EOF  
            Lex_TokenTree *tree = &c->tree_cursor.stream.items[c->tree_cursor.stream.count - 1];
            return tree->Tt_Token;
        }
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
        Lex_Token token = tree->Tt_Token;
        if (token.kind == Tk_LineComment || token.kind == Tk_BlockComment) {
            return _cursor_next(c);
        }
        return token;
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
Ast_Expr *parse_expr_assoc(Parser *p, int min_prec);

Ast_Path parse_path(Parser *p) {
    Ast_Path path = {0};
    Lex_Span span = {
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
    path.span = span;

    return path;
}

Ast_Block *parse_block(Parser *p);
Ast_Expr *make_block_expr(Ast_Block *block) {
    return New(create_expr(Block)(block->span, { .block = block }));
}

static
Ast_Expr *parse_if_expr(Parser *p) {
    Lex_Pos start = p->token.span.start;
    next_token(p); // skip `if`
    
    Ast_Expr *cond = parse_expr_assoc(p, 0);
    Ast_Block *body = parse_block(p);

    Lex_Span span = {
        .start = start,
        .end = body->span.end,
        .filename = body->span.filename
    };

    Ast_Expr *if_expresssion = New(create_expr(If)(span, { .condition = cond, .if_branch = body }));
    if (p->token.kind == Tk_Keyword && p->token.Tk_Keyword.keyword == K_Else) {
        next_token(p); // skip `else`
        Ast_Expr *else_branch = NULL;
        if (p->token.kind == Tk_Keyword && p->token.Tk_Keyword.keyword == K_If)  {
            else_branch = parse_if_expr(p);
        } else {
            else_branch = make_block_expr(parse_block(p));
        }
        if_expresssion->If.else_block = else_branch;
    }
    return if_expresssion;
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
                case K_If:
                    return parse_if_expr(p);
                default: break;
            }
        } break;
        case '(': {
            // TODO: parsing tuples and arrow functions () -> something
            next_token(p);
            Ast_Expr *expr = parse_expr_assoc(p, 0);
            Lex_Token endtoken = expect(p, ')');
            Lex_Span span = {
                .start = token.span.start,
                .end = endtoken.span.end,
                .filename = token.span.filename
            };
            return New(create_expr(Paren)(span, { .expr = expr }));
        } break;
        case '{': {
            Ast_Block *block = parse_block(p);
            return New(create_expr(Block)(block->span, { .block = block }));
        } break;
        case Tk_Ident: {
            Ast_Path path = parse_path(p);
            return New(create_expr(Path)(path.span, { .path = path }));
        } break;
        default: break;
    }
    assert(false && "SyntaxError: expected string, char, number, boolean, nil or identifier");
defer: 
    next_token(p);
    return return_value;
}

static
Ast_Expr *parse_subscript(Parser *p, Ast_Expr *base) {
    expect(p, (Lex_TokenKind)'[');

    // TODO: implement subscirpt multiple arguments (auto tuple generation)
    Ast_Expr *subscript = parse_expr_assoc(p, 0);
    Lex_Pos end = expect(p, (Lex_TokenKind)']').span.end;

    Lex_Span span = {
        .start = base->span.start,
        .end = end,
        .filename = base->span.filename
    };
    return New(create_expr(Subscript)(span, { .base = base, .subscript = subscript }));
}

static
Ast_Expr *parse_call(Parser *p, Ast_Expr *base) {
    expect(p, (Lex_TokenKind)'(');

    Ast_Exprs arguments = {0};
    if (p->token.kind == ')') {
        goto end;
    }
    while (true) {
        Ast_Expr *arg = parse_expr_assoc(p, 0);
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
    Lex_Pos end = p->token.span.end;
    next_token(p); // skip )
    Lex_Span span = {
        .start = base->span.start,
        .end = end,
        .filename = base->span.filename
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
            *matched = true;
            return parse_subscript(p, base);
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
Ast_Expr *parse_expr_assoc(Parser *p, int min_prec) {
    Ast_Expr *lhs = parse_expr_prefix(p);

    AssocOp op;
    while (is_associative_operator(p, &op)) {
        int prec = op.precedence;
        if (prec < min_prec) {
            break;
        }
        next_token(p);

        // TODO: detect chained comparison

        Ast_Expr *rhs = parse_expr_assoc(p, prec + op.accociativity);
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

bool is_block_expr(Ast_ExprKind kind) {
    return kind == If_kind || kind == Block_kind;
}

Ast_Type *parse_type(Parser *p);
Ast_Type *parse_generic(Parser *p, Ast_Type *base);

Ast_Type *parse_type(Parser *p) {
    Lex_Token token = p->token;
    Ast_Type *ty = NULL;

    switch ((int)token.kind) {
        case '|': {
            next_token(p);
            Ast_Type *inner = parse_type(p);
            Lex_Pos end = expect(p, '|').span.end;
            Lex_Span span = {
                .start = token.span.start,
                .end = end,
                .filename = token.span.filename
            };
            return New(create_type(Owned)(span, { .ty = inner }));
        } break;
        case Tk_Ident: {
            Ast_Path path = parse_path(p);
            ty = New(create_type(TyPath)(path.span, { .path = path }));
        } break;
        case '[': {
            next_token(p);
            bool is_slice = true;
            size_t size = 0;
            if (p->token.kind == Tk_Number) {
                is_slice = false;
                size = p->token.Tk_Number.number.integer;
                next_token(p);
            }
            expect(p, ']');
            Ast_Type *ty = parse_type(p);
            Lex_Span span = {
                .start = token.span.start,
                .end = ty->span.end,
                .filename = token.span.filename
            };
            if (is_slice) {
                return New(create_type(TySlice)(span, { .ty = ty }));
            } else {
                return New(create_type(TyArray)(span, { .ty = ty, .size = size }));
            }
        } break;
        case '(': {
            next_token(p);
            Ast_Tys types = {0};
            while (true) {
                Ast_Type *tuple_arg = parse_type(p);
                if (p->token.kind == ')') {
                    if (types.count == 0)
                        ty = tuple_arg;
                    else
                        da_append(&types, tuple_arg);
                    break;
                } else if (p->token.kind == ',') {
                    next_token(p);
                    da_append(&types, tuple_arg);
                }
            }
            Lex_Pos end = p->token.span.end;
            next_token(p);
            if (ty == NULL) {
                Lex_Span span = {
                    .start = token.span.start,
                    .end = end,
                    .filename = token.span.filename
                };
                ty = New(create_type(TyTuple)(span, { .types = types }));
            }
        } break;
        case '&':
        case '*': {
            next_token(p);
            Ast_Mutability mut = M_Const;
            if (p->token.kind == Tk_Keyword && p->token.Tk_Keyword.keyword == K_Let) {
                mut = M_Mut;
                next_token(p);
            }
            Ast_Type *ty = parse_type(p);

            Lex_Pos end = ty->span.end;
            bool nullable = false;
            if (ty->kind == Nullable_kind) {
                Ast_Type *inner = ty->Nullable.ty;
                free(ty);
                ty = inner;
                nullable = true;
            }
            Lex_Span span =  {
                .start = token.span.start,
                .end = end,
                .filename = token.span.filename
            };
            if (token.kind == '&') {
                return New(create_type(Ref)(span, { .ty = ty, .mut = mut, .nullable = nullable }));
            }
            return New(create_type(Ptr)(span, { .ty = ty, .mut = mut, .nullable = nullable }));
        } break; 
        default:
            assert(false && "Not a valid token to start a type");
            break;
    }

    if (p->token.kind == '(') {
        ty = parse_generic(p, ty);
    }

    if (p->token.kind == '?') {
        Lex_Span span = {
            .start = ty->span.start,
            .end = p->token.span.end,
            .filename = p->token.span.filename,
        };
        ty = New(create_type(Nullable)(span, { .ty = ty }));
        next_token(p);
    }

    return ty;
}

Ast_Type *parse_generic(Parser *p, Ast_Type *base) {
    assert(false && "parsing of genric tys not implemented");
}

Ast_Stmt *parse_decl_statement(Parser *p) {
    assert(p->token.kind == Tk_Keyword && "Keyword Token is required");
    Ast_Mutability mut;
    switch (p->token.Tk_Keyword.keyword) {
        case K_Const:
            mut = M_Const;
            break;
        case K_Let:
            mut = M_Mut;
            break;
        default:
            assert(false && "Invalid parser; const or let token");
            break;
    }
    next_token(p);
    Lex_Span start = p->token.span;
    String_Builder ident = expect(p, Tk_Ident).Tk_Ident.name;

    Ast_Type *type = NULL;
    if (p->token.kind != '=' && p->token.kind != ';') {
        type = parse_type(p);
    } else {
        Lex_Span span = {
            .filename = start.filename
        };
        type = New(create_type(Inferred)(span, {}));
    }

    Ast_Expr *init = NULL;
    if (p->token.kind == '=') {
        next_token(p);
        init = parse_expr_assoc(p, 0);
    }

    Lex_Pos end = expect(p, ';').span.end;
    Lex_Span span = {
        .start = start.start,
        .end = end,
        .filename = start.filename
    };
    return New(create_stmt(Decl)(span, { .mut = mut, .ident = ident, .init = init, .type = type }));
}

Ast_Stmt *parse_stmt(Parser *p) {
    while (true) {
        // TODO: generate redundant semicolons warning
        if (p->token.kind != ';') {
            break;
        }
        next_token(p);
    }
    if (p->token.kind == Tk_Keyword) {
        switch (p->token.Tk_Keyword.keyword) {
            case K_Const:
            case K_Let: {
                return parse_decl_statement(p);
            } break;
            default: break;
        }
    }
    Ast_Expr *expr = parse_expr_assoc(p, 0);
    Lex_Pos end;
    bool block_expr = is_block_expr(expr->kind);
    if (!block_expr) {
        end = expect(p, ';').span.end;
    } else {
        end = expr->span.end;
    }
    Lex_Span span = {
        .start = expr->span.start,
        .end = end,
        .filename = expr->span.filename
    };
    return New(create_stmt(Expr)(span, { .expr = expr, .semicolon = !block_expr }));
}

Ast_Block *parse_block(Parser *p) {
    Lex_Pos start = expect(p, '{').span.start;
    Ast_Stmts stmts = {0};

    bool is_empty_block = p->token.kind == '}';
    while (!is_empty_block) {
        Ast_Stmt *stmt = parse_stmt(p);
        da_append(&stmts, stmt);

        if (p->token.kind == '}') {
            break;
        }
    }
    Lex_Span endspan = p->token.span;
    next_token(p); // skip }
    Lex_Span span = {
        .start = start,
        .end = endspan.end,
        .filename = endspan.filename
    };
    return New(((Ast_Block) { .stmts = stmts, .span = span }));
}

Ast_Item *parse_directive_item(Parser *p) {
    Lex_Token token = p->token;
    switch (token.Tk_Directive.directive) {
        case D_Entrypoint: {
            next_token(p);
            Ast_Block *block = parse_block(p);
            Lex_Span span = {
                .start = token.span.start,
                .end = block->span.end,
                .filename = token.span.filename
            };
            return New(create_item(RunBlock)(span, { .block = block }));
        } break;
        case D_Open:
        case D_Include:
        case D_If:
            assert(false && "#open, #include and #if not implemented yet");
            break;
        default:
            assert(false && "Unreachable");
    }
}

Ast_Source parse_source(Parser *p) {
    Ast_Source source = {0};
    while (true) {
        Lex_Token token = p->token;
        if (token.kind == Tk_EOF) {
            break;
        }
        switch ((int)token.kind) {
            case Tk_Directive: {
                Ast_Item *item = parse_directive_item(p);
                da_append(&source, item);
            } break;
            default:
                assert(false && "Unkown token at top-level of module");
        }
    }
    return source;
}

Ast_Source parser_parse_source(Lex_TokenStream stream) {
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
    return parse_source(&p);
}

