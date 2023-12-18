#ifndef AST_H_
#define AST_H_

#include "lexer.h"
#include "strings.h"

// FIXME: Ident should be a struct containing the 
//        definition span and the name should be stored
//        as an index in a symbol table

typedef struct _Ast_Expr Ast_Expr;

typedef struct {
    Ast_Expr **items;
    size_t count;
    size_t capacity;
} Ast_Exprs;

typedef struct {
    String_Builder ident;
    // TODO: Support Generic Arguments
} Ast_PathSegment;

typedef struct {
    Ast_PathSegment *items;
    size_t count;
    size_t capacity;
} Ast_Path;

#define ENUMERATE_EXPR_NODES                \
    _NODE(Literal, {                        \
        enum {                              \
            L_String,                       \
            L_Char,                         \
            L_Integer,                      \
            L_Float,                        \
            L_Boolean,                      \
            L_Nil                           \
        } kind;                             \
        union {                             \
            String_Builder string;          \
            uint32_t wchar;                 \
            bool boolean;                   \
            uint32_t integer;               \
            double floating;                \
        };                                  \
        Lex_NumberClass nclass;             \
    })                                      \
    _NODE(Path, {                           \
        Ast_Path path;                      \
    })                                      \
    _NODE(Unary, {                          \
        UnaryOp op;                         \
        Ast_Expr *expr;                     \
    })                                      \
    _NODE(Call, {                           \
        Ast_Expr *function;                 \
        Ast_Exprs arguments;                \
    })                                      \
    _NODE(Member, {                         \
        Ast_Expr *expr;                     \
        String_Builder ident;               \
    })                                      \
    _NODE(Paren, { Ast_Expr *expr; })       \
    _NODE(Binary, {                         \
        BinaryOp op;                        \
        Ast_Expr *lhs;                      \
        Ast_Expr *rhs;                      \
    })                                      \
    _NODE(Assign, {                         \
        AssignmentOp op;                    \
        Ast_Expr *lhs;                      \
        Ast_Expr *rhs;                      \
    })                                      \
    _NODE(Refrence, {                       \
        Ast_Expr *expr;                     \
    })                                      \

typedef enum {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_EXPR_NODES
#undef _NODE
    Expr_NumberOfElements
} Ast_ExprKind;

struct _Ast_Expr {
    Ast_ExprKind kind;
    union {
#define _NODE(name, ...) struct __VA_ARGS__ name;
        ENUMERATE_EXPR_NODES
#undef _NODE
    };
    Lex_Span span;
};

#define _new_expr1(variant) (Ast_Expr){ .kind = variant##_kind, .variant =
#define _new_expr2(_span, ...) __VA_ARGS__, .span = (_span) }
#define create_expr(variant) _new_expr1(variant)_new_expr2

#define intermediate(...) intermediate_inter(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define intermediate_inter(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, count, ...) \
    intermediate_##count(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)

#define _asg(name) typeof(__variant.name) name = __variant.name;
#define intermediate_1(a1, ...) _asg(a1)
#define intermediate_2(a1, a2, ...) _asg(a1) _asg(a2)
#define intermediate_3(a1, a2, a3, ...) \
    _asg(a1) _asg(a2) _asg(a3)
#define intermediate_4(a1, a2, a3, a4, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4)
#define intermediate_5(a1, a2, a3, a4, a5, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5)
#define intermediate_6(a1, a2, a3, a4, a5, a6, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5) \
    _asg(a6)
#define intermediate_7(a1, a2, a3, a4, a5, a6, a7, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5) \
    _asg(a6) _asg(a7)
#define intermediate_8(a1, a2, a3, a4, a5, a6, a7, a8, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5) \
    _asg(a6) _asg(a7) _asg(a8)
#define intermediate_9(a1, a2, a3, a4, a5, a6, a7, a8, a9, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5) \
    _asg(a6) _asg(a7) _asg(a8) _asg(a9)
#define intermediate_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) \
    _asg(a1) _asg(a2) _asg(a3) _asg(a4) _asg(a5) \
    _asg(a6) _asg(a7) _asg(a8) _asg(a9) _asg(a10)

#define bswitch(expr, ...) { typeof((expr)) __expr = (expr); switch (__expr->kind) __VA_ARGS__ }
#define bind(name, ...) \
    case name##_kind: { typeof(__expr->name) __variant = __expr->name; intermediate __VA_ARGS__ } break;

void ast_print_expr(String_Builder *sb, Ast_Expr *expr, uint32_t level);

#endif //AST_H_
