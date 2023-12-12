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
    })

#define ENUMERATE_LEXER_ERRORS       \
    _E(ParserUninitialized)          \
    _E(UnexpectedCharacter)          \
    _E(UnclosedCharLiteral)          \
    _E(UnclosedStringLiteral)        \
    _E(EmptyCharLiteral)             \
    _E(UnclosedMultilineComment)     \
    _E(InvalidNumberSuffix)          \
    _E(InvalidDigitForBase)          \
    _E(MulitCharCharLiteral)         \
    _E(UnexpectedEOF)                \
    _E(DifferentBaseFloatingLiteral) \
    _E(InvalidSuffixForFloat)        \
    _E(SientificFloatWithoutExponent)\
    _E(MultipleDotsInFloat)          \
    _E(UnsupportedDigitForBase)      \
    _E(UnknownPunctuator)            \
    _E(InvalidEscape)                \
    _E(UnknownDirective)             \
    _E(InvalidZeroSizeNote)

typedef enum {
#define _E(name) name,
    ENUMERATE_LEXER_ERRORS
#undef _E
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
    size_t col;
    size_t row;
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

typedef struct _TokenTree Lex_TokenTree;

typedef struct {
    Lex_TokenTree *items;
    size_t count;
    size_t capacity;
} Lex_TokenStream;

typedef enum {
    Dl_Paren = 1,
    Dl_Brace,
    Dl_Bracket,
} Lex_Delimiter;

typedef struct {
    Lex_Delimiter delimiter;
    Lex_TokenStream stream;
    struct {
        Lex_Span open;
        Lex_Span close;
    } span;
} Lex_Delimited;

typedef enum {
    Tt_Token,
    Tt_Delimited
} Lex_TreeType;

struct _TokenTree {
    Lex_TreeType type;
    union {
        Lex_Token Tt_Token;
        Lex_Delimited Tt_Delimited;
    };
};

Lex_TokenStream lexer_tokenize_source(String_View filename, String_View content);

void lexer_token_free(Lex_Token token);
void lexer_token_stream_free(Lex_TokenStream stream);

void lexer_print_pos(String_Builder *sb, Lex_Pos pos);
void lexer_print_span(String_Builder *sb, Lex_Span span);
void lexer_print_token(String_Builder *sb, Lex_Token *token);
void lexer_print_delimited(String_Builder *sb, Lex_Delimited *token);

#endif // LEXER_H_
