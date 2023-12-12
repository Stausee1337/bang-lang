#ifndef LEXER_NAMES_H_
#define LEXER_NAMES_H_

#include "lexer.h"

inline static
const char *lexer_tokenkind_to_string(Lex_TokenKind in) {
    static const char *lexer_token_names[] = {
        "EOF",
#define VARIANT(name, ...) #name,
        ENUMERATE_LEXER_TOKENS
#undef VARIANT
    };
    assert(in < Tk_NumberOfTokens);

    return lexer_token_names[in - Tk_EOF];
}

inline static
const char *lexer_error_to_string(Lex_Error in) {
    static const char *lexer_error_names[] = {
#define _E(name) #name,
        ENUMERATE_LEXER_ERRORS
#undef _E
    };
    assert(in < NumberOfErrors);

    return lexer_error_names[in];
}


#endif // LEXER_NAMES_H_ 
