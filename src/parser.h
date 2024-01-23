#ifndef PARSER_H_
#define PARSER_H_

#include "AST.h"
#include "lexer.h"

Ast_Stmt *parser_parse(Lex_TokenStream stream);

#endif // PRASER_H_
