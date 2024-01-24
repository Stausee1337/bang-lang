#ifndef PARSER_H_
#define PARSER_H_

#include "AST.h"
#include "lexer.h"

Ast_Source parser_parse_source(Lex_TokenStream stream);

#endif // PRASER_H_
