#ifndef LEXER_H_
#define LEXER_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "strings.h"
#include "lexerc.generated.h"
#include "operators.generated.h"

#define lex_tok_typ(tok)    (tok)->base.type
#define lex_tok_sv(tok)    (tok)->base.window
#define lex_tok_start(tok)    (tok)->base.start
#define lex_tok_end(tok)    (tok)->base.end

#define lex_tok_cast(Typ, tok)   ((Typ*)(tok))
#define lex_tok_err(tok)    lex_tok_cast(Lex_TokenError, tok)->error
#define lex_tok_is_punct(tok)  (lex_tok_typ(tok) < 0x80 || lex_tok_typ(tok) > 0xff)

typedef enum {
    ERROR = 0x80,
    EOS,

    COMMENT,

    // Literals
    CHAR,
    STRING,
    NOTE_STR,
    NUMBER,

    IDENT,
    KEYWORD,
    DIRECTIVE,

    // End
    NumberOfTokens
} Lex_TokenType;

typedef NumberClass Lex_NumberClass;
typedef Directive Lex_Directive;
typedef Keyword Lex_Keyword;

#define IS_FLOAT_CLASS(class) \
    (class) == Nc_f32 || (class) == Nc_f64 || (class) == Nc_FloatingPointNumber

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


typedef struct {
    uint32_t col;
    uint32_t row;
} Lex_Pos;

union _64_bit_number {
    uint64_t integer;
    double floating;
};
typedef struct {
    String_View input;

    size_t input_pos;
    Lex_Pos file_pos;

    size_t token_start;
    size_t token_end;

    Lex_Pos token_start_pos;
    Lex_Pos token_end_pos;

    Lex_TokenType token;
    String_View window;

    Lex_Error error;
    char char_literal;
    union _64_bit_number number;
    Lex_NumberClass number_class;
    Lex_Directive directive;
    Lex_Keyword keyword;
} Lex_State;

typedef struct {
    Lex_TokenType type;
    String_View window;
    Lex_Pos start;
    Lex_Pos end;
} Lex_TokenBase;

typedef struct {
    Lex_TokenBase base;
    Lex_Error error;
} Lex_TokenError;

typedef struct {
    Lex_TokenBase base;
    union _64_bit_number data;
    Lex_NumberClass nclass;
} Lex_TokenNumber;

typedef struct {
    Lex_TokenBase base;
    Lex_Directive directive;
} Lex_TokenDirective;

typedef struct {
    Lex_TokenBase base;
    Lex_Keyword keyword;
} Lex_TokenKeyword;

typedef struct {
    Lex_TokenBase base;
    char _offset[64 - sizeof(Lex_TokenBase)];
} Lex_Token;


void lexer_init(Lex_State *lexer, String_View sv);
Lex_Token lexer_lex(Lex_State *lexer);
void lexer_print_token(String_Builder *sb, Lex_Token *token);

#endif // LEXER_H_
