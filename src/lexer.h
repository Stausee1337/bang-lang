#ifndef LEXER_H_
#define LEXER_H_

#include <stdint.h>

#include "utf8.h"
#include "dynarray.h"

#define TokenType_Character     0
#define TokenType_Other         1

#define Lexer_TokenType(token) \
    ((int)token) >= 0x100

typedef enum {
    EOS,
    ERROR,
    COMMENT,

    // Literals
    CHAR,
    STRING,

    // Numbers
    FLOAT,
    DOUBLE,
    BYTE,
    UBYTE,
    SHORT,
    USHORT,
    INT,
    UINT,
    LONG,
    ULONG,

    IDENT,

    // Keywords
    TRUE,
    FALSE,
    NULL_KW
} LexToken;

typedef enum {
    NonUTF8Character,
    UnexpectedCharacter,
    UnclosedCharLiteral,
    UnclosedStringLiteral,
    EmptyCharLiteral,
    UnclosedMultilineComment,
    InvalidNumberSuffix,
    InvalidDigitForBase,
    MulitCharCharLiteral,
    UnexpectedEOF
} LexError;

typedef struct {
    Utf8Slice input;
    DynArray linebreak_map;

    size_t input_pos;

    size_t token_start;
    size_t token_length;

    LexError error;
    LexToken token;
    Utf8Slice window;

    union {
        Utf8String string;
        bool boolean;
        uint32_t character;

        float f32;
        double f64;

        int8_t u8;
        uint8_t i8;
        int16_t i16;
        uint16_t u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
    } current_data;
} LexState;

LexState lexer_init(const char *input);
void lexer_lex(LexState *lexer);

#endif // LEXER_H_
