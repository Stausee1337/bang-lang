#ifndef AST_H_
#define AST_H_

#include "lexer.h"

#define ENUMERATE_EXPR_NODES    \
    _NODE(Empty, { })           \

enum Ast_ExprKind {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_EXPR_NODES
#undef _NODE
};

typedef struct {
    enum Ast_ExprKind kind;
    union {
#define _NODE(name, body) struct body name;
        ENUMERATE_EXPR_NODES
#undef _NODE
    };
    Lex_Span span;
} Ast_Expr;

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

#endif //AST_H_
