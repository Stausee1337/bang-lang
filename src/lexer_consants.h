#ifndef LEXER_CONSTANTS_H_
#define LEXER_CONSTANTS_H_

#include "lexer.h"

static const char *lexer_token_names[] = {
    "Error",
    "EOS",

    "Comment",

    "Char",
    "String",
    "Note",
    "Number",

    "Identifier",
    "Keyword",
    "Directive"
};

static_assert(NumberOfTokens == 0x8a, "Tokens have now changed");

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

static const char *lexer_number_classes[] = {
    "i8",
    "u8",
    "i16",
    "u16",
    "i32",
    "u32",
    "i64",
    "u64",
    "isize",
    "usize",
    "f32",
    "f64",
    "Number",
    "FloatingPointNumber"
};

static_assert(NumberOfClasses == 14, "Classes have now changed");

static const char* lexer_2chr_operators[] = {
    TOK_TO_STR(==),
    TOK_TO_STR(!=),
    TOK_TO_STR(<=),
    TOK_TO_STR(>=),

    TOK_TO_STR(+=),
    TOK_TO_STR(-=),
    TOK_TO_STR(*=),
    TOK_TO_STR(/=),
    TOK_TO_STR(%=),

    TOK_TO_STR(&=),
    TOK_TO_STR(|=),
    TOK_TO_STR(^=),

    TOK_TO_STR(<<),
    TOK_TO_STR(<<),

    TOK_TO_STR(:=),
    TOK_TO_STR(::),
    TOK_TO_STR(->),
    0
};

static const char* lexer_3chr_operators[] = {
    TOK_TO_STR(>>=),
    TOK_TO_STR(<<=),
    0
};

static const char* lexer_directives[] = {
    "if",
    "else",

    "open",
    "load",

    "zero",

    NULL
};

static_assert(Dr_Count == 5, "Number of directives changed");

static const char* lexer_keywords[] = {
    "nil",
    "true",
    "false",

    "if",
    "else",

    "for",
    "loop",
    "while",

    "break",
    "continue",

    "fn",
    "enum",
    "struct",
    "variant",
    
    NULL
};

static_assert(Kw_Count == 14, "Number of keywords changed");

#endif // LEXER_CONSTANTS_H_ 

