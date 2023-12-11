#ifndef LEXER_H_
#define LEXER_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "strings.h"
#include "lexerc.generated.h"
#include "operators.generated.h"

typedef NumberClass Lex_NumberClass;
typedef Directive Lex_Directive;
typedef Keyword Lex_Keyword;

#define ENUMERATE_LEXER_TOKENS      \
    VARIANT(Comment)                \
    VARIANT(Error, {                \
        Lex_Error error;            \
    })                              \
    VARIANT(Char, {                 \
        uint32_t wchar;             \
    })                              \
    VARIANT(String, {               \
        String_Builder string;      \
    })                              \
    VARIANT(Note, {                 \
        String_Builder note;        \
    })                              \
    VARIANT(Number, {               \
        union _64_bit_number number;\
        Lex_NumberClass nclass;     \
    })                              \
    VARIANT(Ident, {                \
        String_Builder name;        \
    })                              \
    VARIANT(Keyword, {              \
        Lex_Keyword keyword;        \
    })                              \
    VARIANT(Directive, {            \
        Lex_Directive directive;    \
    })                              \

typedef enum {
    ParserUninitialized,
    UnexpectedCharacter,
    UnclosedCharLiteral,
    UnclosedStringLiteral,
    EmptyCharLiteral,
    UnclosedMultilineComment,
    InvalidNumberSuffix,
    InvalidDigitForBase,
    MulitCharCharLiteral,
    UnexpectedEOF,
    DifferentBaseFloatingLiteral,
    InvalidSuffixForFloat,
    SientificFloatWithoutExponent,
    MultipleDotsInFloat,
    UnsupportedDigitForBase,
    UnknownPunctuator,
    InvalidEscape,
    UnknownDirective,
    InvalidZeroSizeNote,

// META:
    NumberOfErrors,
} Lex_Error;

typedef enum {
#define VARIANT(name, ...) Tk_##name,
    Tk_EOF = 0x80,
ENUMERATE_LEXER_TOKENS
    Tk_NumberOfTokens
#undef VARIANT
} Lex_TokenKind;

#define IS_FLOAT_CLASS(class) \
    (class) == Nc_f32 || (class) == Nc_f64 || (class) == Nc_FloatingPointNumber

typedef struct {
    uint32_t col;
    uint32_t row;
} Lex_Pos;

typedef struct {
    String_View filename;
    Lex_Pos start;
    Lex_Pos end;
} Lex_Span;

union _64_bit_number {
    uint64_t integer;
    double floating;
};

#define VARIANT(...) VARIANT_REAL(__VA_ARGS__, 2, 1)
#define VARIANT_REAL(name, body, count, ...) VARIANT_##count(name, body)

#define VARIANT_1(name, ...)
#define VARIANT_2(name, body) \
    typedef struct body Lex_Token##name;

ENUMERATE_LEXER_TOKENS

#undef VARIANT_2

typedef struct {
    Lex_Span span;
    Lex_TokenKind kind;
    union {
#define VARIANT_2(name, body) Lex_Token##name Tk_##name;
        ENUMERATE_LEXER_TOKENS
    };
} Lex_Token;

#undef VARIANT_2
#undef VARIANT_1
#undef VARIANT_REAL
#undef VARIANT

typedef struct {
    String_View filename;
    String_View input;

    size_t input_pos;
    Lex_Pos file_pos;

    size_t token_start;
    size_t token_end;

    Lex_Pos token_start_pos;
    Lex_Pos token_end_pos;

    String_View window;

    Lex_Token token;
} Lex_State;

void lexer_init(Lex_State *lexer, String_View sv);
// Lex_Token lexer_lex(Lex_State *lexer);
void lexer_next(Lex_State *lexer);
void lexer_print_token(String_Builder *sb, Lex_Token *token);

#endif // LEXER_H_
