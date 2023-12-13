#ifndef AST_H_
#define AST_H_

#include "lexer.h"

#define ENUMERATE_EXPRESSION_NODES    \
    _NODE(EmptyExpression, {})        \

#define _NODE(name, body) \
    typedef struct body Ast_##name;
ENUMERATE_EXPRESSION_NODES
#undef _NODE

enum _expression_kind {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_EXPRESSION_NODES
#undef _NODE
};

typedef struct {
    enum _expression_kind kind;
    union {
#define _NODE(name, ...) Ast_##name name;
        ENUMERATE_EXPRESSION_NODES
#undef _NODE
    };
    Lex_Span span;
} Ast_ExpressionKind;

#endif //AST_H_
