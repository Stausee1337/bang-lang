#ifndef LEXER_CONSTANTS_H_
#define LEXER_CONSTANTS_H_

#include "lexer.h"

static const char *lexer_token_names[] = {
    "EOS",
    "Comment",
    "Error",
    "Char",
    "String",
    "Note",
    "Number",
    "Identifier",
    "Keyword",
    "Directive"
};

static_assert(Tk_NumberOfTokens == 0x8a, "Tokens have now changed");

static const char *lexer_error_names[] = {
    "ParserUninitialized",
    "UnexpectedCharacter",
    "UnclosedCharLiteral",
    "UnclosedStringLiteral",
    "EmptyCharLiteral",
    "UnclosedMultilineComment",
    "InvalidNumberSuffix",
    "InvalidDigitForBase",
    "MulitCharCharLiteral",
    "UnexpectedEOF",
    "DifferentBaseFloatingLiteral",
    "InvalidSuffixForFloat",
    "SientificFloatWithoutExponent",
    "MultipleDotsInFloat",
    "UnsupportedDigitForBase",
    "UnknownPunctuator",
    "InvalidEscape",
    "UnknownDirective",
    "InvalidZeroSizeNote",
};

static_assert(NumberOfErrors == 19, "Errors have now changed");

#endif // LEXER_CONSTANTS_H_ 
