#include <stdio.h>

#include "AST.h"

static 
const char *expr_to_string(Ast_ExprKind kind) {
#define _NODE(name, ...) #name,
    static const char *expr_names[] = {
        ENUMERATE_EXPR_NODES
    };
#undef _NODE
    assert(kind < Expr_NumberOfElements);
    return expr_names[kind];
}

static 
const char *stmt_to_string(Ast_StmtKind kind) {
#define _NODE(name, ...) #name,
    static const char *stmt_names[] = {
        ENUMERATE_STMT_NODES
    };
#undef _NODE
    assert(kind < Stmt_NumberOfElements);
    return stmt_names[kind];
}

static 
const char *item_to_string(Ast_ItemKind kind) {
#define _NODE(name, ...) #name,
    static const char *item_names[] = {
        ENUMERATE_ITEM_NODES
    };
#undef _NODE
    assert(kind < Item_NumberOfElements);
    return item_names[kind];
}

#define INDENT "    "
void indent(String_Builder *sb, uint32_t level) {
    for (uint32_t i = 0; i < level; i++) {
        sb_append_cstr(sb, INDENT);
    }
}

void ast_print_expr(String_Builder *sb, Ast_Expr *expr, uint32_t level) {
    sb_append_cstr(sb, expr_to_string(expr->kind));
    sb_append_cstr(sb, " { ");

    sb_append_cstr(sb, "span = ");
    lexer_print_span(sb, expr->span);

    bswitch(expr, {
        bind(Literal, (kind, string, wchar, boolean, integer, floating, nclass) {
            char buffer[100] = {0};
            switch (kind) {
                case L_String:
                    sb_append_cstr(sb, ", string = ");
                    da_append_many(sb, string.items, string.count);
                    break;
                case L_Char:
                    sb_append_cstr(sb, ", char = ");
                    da_append(sb, (char)wchar);
                    break;
                case L_Boolean:
                    sb_append_cstr(sb, ", boolean = ");
                    sb_append_cstr(sb, boolean ? "true" : "false");
                    break;
                case L_Nil:
                    sb_append_cstr(sb, ", nil");
                    break;
                case L_Integer:
                    sprintf(buffer, "%d", integer);
                    sb_append_cstr(sb, ", integer = ");
                    sb_append_cstr(sb, buffer);
                    da_append(sb, ':');
                    sb_append_cstr(sb, number_class_to_string(nclass));
                    break;
                case L_Float:
                    sprintf(buffer, "%f", floating);
                    sb_append_cstr(sb, ", float = ");
                    sb_append_cstr(sb, buffer);
                    da_append(sb, ':');
                    sb_append_cstr(sb, number_class_to_string(nclass));
                    break;
            };
        });
        bind(Path, (path) {
            sb_append_cstr(sb, ", path = ");
            for (size_t i = 0; i < path.count; i++) {
                Ast_PathSegment *segment = &path.items[i];
                da_append_many(sb, segment->ident.items, segment->ident.count);
                if (i < path.count - 1) {
                    da_append(sb, ':');
                }
            }
        });
        bind(Unary, (op, expr) {
            sb_append_cstr(sb, ", op = UnaryOp::");
            sb_append_cstr(sb, unary_op_to_string(op));
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "expr = ");
            ast_print_expr(sb, expr, level + 1);
        });
        bind(Call, (function, arguments) {
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "function = ");
            ast_print_expr(sb, function, level + 1);

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "arguments = [");
            for (size_t i = 0; i < arguments.count; i++) {
                da_append(sb, '\n');
                indent(sb, level+2);
                Ast_Expr *arg = arguments.items[i];
                ast_print_expr(sb, arg, level + 2);
                da_append(sb, ',');
            }
            sb_append_cstr(sb, "]");
        });
        bind(Member, (expr, ident) {
            sb_append_cstr(sb, ", ident = ");
            da_append_many(sb, ident.items, ident.count);
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "expr = ");
            ast_print_expr(sb, expr, level + 1);
        });
        bind(Paren, (expr) {
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "expr = ");
            ast_print_expr(sb, expr, level + 1);
        });
        bind(Binary, (op, lhs, rhs) {
            sb_append_cstr(sb, ", op = BinaryOp::");
            sb_append_cstr(sb, binary_op_to_string(op));
 
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "lhs = ");
            ast_print_expr(sb, lhs, level + 1);

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "rhs = ");
            ast_print_expr(sb, rhs, level + 1);
        });
        bind(Assign, (op, lhs, rhs) {
            sb_append_cstr(sb, ", op = AssignmentOp::");
            sb_append_cstr(sb, assignment_op_to_string(op));
 
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "lhs = ");
            ast_print_expr(sb, lhs, level + 1);

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "rhs = ");
            ast_print_expr(sb, rhs, level + 1);
        });
        bind(Refrence, (expr) {
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "expr = ");
            ast_print_expr(sb, expr, level + 1);
        });
        bind(If, (condition, if_branch, else_block) {
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "condition = ");
            ast_print_expr(sb, condition, level + 1);

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "if_branch = ");
            ast_print_block(sb, if_branch, level + 1);

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "else_block = ");
            if (else_block != NULL) {
                ast_print_expr(sb, else_block, level + 1);
            } else {
                sb_append_cstr(sb, "NULL");
            }
        });
        bind(Block, (block) {
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "block = ");
            ast_print_block(sb, block, level + 1);
        });
        default: break;
    });

    sb_append_cstr(sb, " }");
}

void ast_print_stmt(String_Builder *sb, Ast_Stmt *stmt, uint32_t level) {
    sb_append_cstr(sb, stmt_to_string(stmt->kind));
    sb_append_cstr(sb, " { ");

    sb_append_cstr(sb, "span = ");
    lexer_print_span(sb, stmt->span);

    bswitch(stmt, {
        bind(Expr, (expr, semicolon) {
            sb_append_cstr(sb, ", semi = ");
            sb_append_cstr(sb, semicolon ? "true" : "false");
            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "expr = ");
            ast_print_expr(sb, expr, level + 1);
        });
        bind(Decl, (init, ident, mut, type) {
            sb_append_cstr(sb, ", ident = ");
            da_append_many(sb, ident.items, ident.count);

            sb_append_cstr(sb, ", mut = ");
            sb_append_cstr(sb, mut == M_Mut ? "Mut" : "Const");

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "init = ");
            if (init != NULL) {
                ast_print_expr(sb, init, level + 1);
            } else {
                sb_append_cstr(sb, "NULL");
            }

            sb_append_cstr(sb, ",\n");
            indent(sb, level + 1);
            sb_append_cstr(sb, "type = ");
            if (type->kind == Inferred_kind) {
                sb_append_cstr(sb, "Inferred");
            } else {
                assert(false && "Pretty printing of real types is not implemted yet");
                // ast_print_type(sb, type, level + 1);
            }
        });
        default: break;
    });

    sb_append_cstr(sb, " }");
}

void ast_print_item(String_Builder *sb, Ast_Item *item, uint32_t level) {
    sb_append_cstr(sb, item_to_string(item->kind));
    sb_append_cstr(sb, " { ");

    sb_append_cstr(sb, "span = ");
    lexer_print_span(sb, item->span);

    bswitch(item, {
        bind(RunBlock, (block) {
            sb_append_cstr(sb, ", ");
            ast_print_block(sb, block, level + 1);
        });
        default: break;
    });

    sb_append_cstr(sb, " }");
}

void ast_print_block(String_Builder *sb, Ast_Block *block, uint32_t level) {
    sb_append_cstr(sb, "Block [\n");

    for (size_t i = 0; i < block->stmts.count; i++) {
        Ast_Stmt *stmt = block->stmts.items[i];
        indent(sb, level + 1);
        ast_print_stmt(sb, stmt, level + 1);
        sb_append_cstr(sb, ",\n");
    }
    indent(sb, level);
    sb_append_cstr(sb, "]");
}

void ast_print_source(String_Builder *sb, Ast_Source *source, uint32_t level) {
    sb_append_cstr(sb, "Source [\n");

    for (size_t i = 0; i < source->count; i++) {
        Ast_Item *item = source->items[i];
        indent(sb, level + 1);
        ast_print_item(sb, item, level + 1);
        sb_append_cstr(sb, ",\n");
    }
    indent(sb, level);
    sb_append_cstr(sb, "]");
}
