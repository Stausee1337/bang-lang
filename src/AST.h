#ifndef AST_H_
#define AST_H_

#include "lexer.h"
#include "strings.h"

// FIXME: Ident should be a struct containing the 
//        definition span and the name should be stored
//        as an index in a symbol table

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
    _NODE(Subscript, {                      \
        Ast_Expr *base;                     \
        Ast_Expr *subscript;                \
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
    _NODE(If, {                             \
        Ast_Expr *condition;                \
        Ast_Block* if_branch;               \
        Ast_Expr* else_block;               \
    })                                      \
    _NODE(Block, {                          \
        Ast_Block* block;                   \
    })                                      \

typedef struct _Ast_Expr Ast_Expr;
typedef struct _Ast_Block Ast_Block;

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
    Lex_Span span;
} Ast_Path;

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

typedef struct _Ast_Type Ast_Type;

typedef enum { M_Const, M_Mut } Ast_Mutability;

typedef struct {
    Ast_Type **items;
    size_t count;
    size_t capacity;
} Ast_Tys;

// TODO: maybe add dynamic array
// TODO: missing anonymous struct, enums, ...
// TODO: add union-fallables `Success!Error`
// TODO: add duck-typing match delcartions
#define ENUMERATE_TYPE_NODES                \
    _NODE(TyPath, { Ast_Path path; })       \
    _NODE(Owned, { Ast_Type *ty; })         \
    _NODE(Ref, {                            \
        Ast_Type *ty;                       \
        Ast_Mutability mut;                 \
        bool nullable;                      \
    })                                      \
    _NODE(Ptr, {                            \
        Ast_Type *ty;                       \
        Ast_Mutability mut;                 \
        bool nullable;                      \
    })                                      \
    _NODE(Generic, {                        \
        Ast_Type *base;                     \
        Ast_Tys arguments;                  \
    })                                      \
    _NODE(TyArray, {                        \
        Ast_Type *ty;                       \
        size_t size;                        \
    })                                      \
    _NODE(TySlice, { Ast_Type *ty; })       \
    _NODE(TyTuple, {                        \
        Ast_Tys types;                      \
    })                                      \
    _NODE(Inferred, { })                    \
    _NODE(Nullable, {                       \
        Ast_Type *ty;                       \
    })                                      \

typedef enum {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_TYPE_NODES
#undef _NODE
    Type_NumberOfElements
} Ast_TypeKind;

struct _Ast_Type {
    Ast_TypeKind kind;
    union {
#define _NODE(name, ...) struct __VA_ARGS__ name;
        ENUMERATE_TYPE_NODES
#undef _NODE
    };
    Lex_Span span;
};

#define ENUMERATE_STMT_NODES                \
    _NODE(Expr, {                           \
        Ast_Expr *expr;                     \
        bool semicolon;                     \
    })                                      \
    _NODE(Decl, {                           \
        Ast_Mutability mut;                 \
        String_Builder ident;               \
        Ast_Expr *init;                     \
        Ast_Type* type;                     \
    })                                      \

typedef struct _Ast_Stmt Ast_Stmt;

typedef struct {
    Ast_Stmt **items;
    size_t count;
    size_t capacity;
} Ast_Stmts;

struct _Ast_Block {
    Ast_Stmts stmts;
    /* TODO: add label identifier */
    Lex_Span span;
};

typedef enum {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_STMT_NODES
#undef _NODE
    Stmt_NumberOfElements
} Ast_StmtKind;

struct _Ast_Stmt {
    Ast_StmtKind kind;
    union {
#define _NODE(name, ...) struct __VA_ARGS__ name;
        ENUMERATE_STMT_NODES
#undef _NODE
    };
    Lex_Span span;
};

#define ENUMERATE_ITEM_NODES                \
    _NODE(RunBlock, {                       \
        Ast_Block *block;                   \
    })                                      \

typedef enum {
#define _NODE(name, ...) name##_kind,
    ENUMERATE_ITEM_NODES
#undef _NODE
    Item_NumberOfElements
} Ast_ItemKind;

typedef struct {
    Ast_ItemKind kind;
    union {
#define _NODE(name, ...) struct __VA_ARGS__ name;
        ENUMERATE_ITEM_NODES
#undef _NODE
    };
    Lex_Span span;
} Ast_Item;

typedef struct {
    Ast_Item **items;
    size_t count;
    size_t capacity;
} Ast_Source;

#define _new_1(Typ, variant) (Typ){ .kind = variant##_kind, .variant =
#define _new_2(_span, ...) __VA_ARGS__, .span = (_span) }
#define create_expr(variant) _new_1(Ast_Expr, variant)_new_2
#define create_stmt(variant) _new_1(Ast_Stmt, variant)_new_2
#define create_item(variant) _new_1(Ast_Item, variant)_new_2
#define create_type(variant) _new_1(Ast_Type, variant)_new_2

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
void ast_print_stmt(String_Builder *sb, Ast_Stmt *stmt, uint32_t level);
void ast_print_item(String_Builder *sb, Ast_Item *item, uint32_t level);
void ast_print_type(String_Builder *sb, Ast_Type *type, uint32_t level);
void ast_print_block(String_Builder *sb, Ast_Block *block, uint32_t level);
void ast_print_source(String_Builder *sb, Ast_Source *source, uint32_t level);
void ast_print_path(String_Builder *sb, Ast_Path *path);

#endif //AST_H_
