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
    VARIANT(LineComment)            \
    VARIANT(BlockComment, {         \
        String_Builder body;        \
    })                              \
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
    _E(InvalidZeroSizeNote)          \
    _E(UnexpectedDelimiter)          \
    _E(MissingDelimiter)             \
    _E(MismatchedDelimiter)

typedef enum {
#define _E(name) name,
    ENUMERATE_LEXER_ERRORS
#undef _E
    NumberOfErrors,
} Lex_Error;

typedef enum {
    Tk_INIT = -1,
    Tk_EOF = 0x80,
#define VARIANT(name, ...) Tk_##name,
ENUMERATE_LEXER_TOKENS
    Tk_NumberOfTokens
#undef VARIANT
} Lex_TokenKind;

#define IS_FLOAT_CLASS(class) \
    (class) == Nc_f32 || (class) == Nc_f64 || (class) == Nc_FloatingPointNumber

#define IS_TOKEN_KIND(kind) \
    ((kind) == -1 || ((kind) >= 0x80 && (kind) <= 0xff))
#define AS_PUNCTUATOR(kind) (char*)(&(kind))

typedef struct {
    size_t col;
    size_t row;
} Lex_Pos;

// FIXME: Memory optimize span
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

typedef struct {
    Lex_Error type;
    Lex_Span span;
} Lex_StreamError;

typedef union {
    Lex_StreamError error; 
    Lex_TokenStream stream;
} Lex_TokenizeResult;

Lex_TokenizeResult lexer_tokenize_source(String_View filename, String_View content, bool *success);

void lexer_token_free(Lex_Token token);
void lexer_token_stream_free(Lex_TokenStream *stream);

void lexer_print_pos(String_Builder *sb, Lex_Pos pos);
void lexer_print_span(String_Builder *sb, Lex_Span span);
void lexer_print_token(String_Builder *sb, Lex_Token *token);
void lexer_print_error(String_Builder *sb, Lex_Error *error);
void lexer_print_delimited(String_Builder *sb, Lex_Delimited *token);

#endif // LEXER_H_
