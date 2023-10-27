#include "lexer.h"
#include "utf8.h"
#include <assert.h>

UTF8Error global_error;

#define IS_WHITESPACE(chr) \
    (chr == ' ' || CHR_IN_RANGE(chr, '\x09', '\x0d'))

#define LINEBREAK(chr) \
    (chr == '\n' || (chr == '\r' && LOOKAHEAD() == '\n' && LEX_BUMP()))

#define GET_CURRENT() \
({ utf8_slice_get_char(&lexer->input, &global_error); })

#define GET_CURRENT_FAIL() \
    ({ \
        uchar result;\
        result = GET_CURRENT(); \
        IF_INPUT_EMPTY { FAIL(UnexpectedEOF); } \
        result; \
    })

#define LOOKAHEAD() \
    ({ \
        Utf8Slice copy = utf8_slice_clone(&lexer->input);\
        if(!utf8_slice_bump(&copy)) { FAIL(NonUTF8Character); } \
        uchar result = utf8_slice_get_char(&lexer->input, &global_error); \
        IF_INPUT_EMPTY { result = INVALID_CHAR; }\
        result; \
    })

#define IF_INPUT_EMPTY \
    if (global_error == Error_Empty)

#define CHR_IN_RANGE(chr, lower, upper) \
    ((lower) <= (chr) && (chr) <= (upper))

#define LEX_BUMP() \
    ({ if (!utf8_slice_bump(&lexer->input)) \
    { FAIL(NonUTF8Character); } \
    else { lexer->token_length++; lexer->input_pos++; }\
    true; })

#define NOMATCH     return false

#define FAIL(err) \
    lexer->token = ERROR; \
    lexer->error = (err); \
    return true;

#define SUCCESS(result) \
    lexer->token = (result); \
    return true; 

#define TRY(expression) \
    if ((expression)) { lexer_slice_window(lexer); return; } \
    else { lexer_restore_from_window(lexer); }

typedef struct{
    bool success;
    LexToken token;
} LexResult;

bool _bump_until_eol_eof(LexState *lexer, uchar serach_char) {
    uchar current;
    while (true) {
        LEX_BUMP();
        current = GET_CURRENT();

        IF_INPUT_EMPTY {
            return false;
        }

        if (LINEBREAK(current)) {
            return false;
        }

        if (current == serach_char) {
            return true;
        }
    }
}

LexState lexer_init(const char* input) {
    LexState state = {
        .input = utf8_slice_new_unchecked(input)
    };
    return state;
}

void lexer_linebreak_add(LexState *lexer) {
    (void)lexer;
}

void lexer_slice_window(LexState *lexer) {
    assert(
        utf8_slice_slice(&lexer->window, 0, lexer->token_length) &&
        "Logical error encountered while slicing");
}

void lexer_save_window(LexState *lexer) {
    lexer->window = lexer->input;
}

void lexer_restore_from_window(LexState *lexer) {
    lexer->input = lexer->window;
}

bool lexer_lex_comment(LexState *lexer) {
    LEX_BUMP();
    uchar current = GET_CURRENT();
    if (current == '/') {
        _bump_until_eol_eof(lexer, INVALID_CHAR);
        SUCCESS(COMMENT);
    } else if (current == '*') {
        uint32_t nesting_levels = 0; 
        while (true) {
            LEX_BUMP();
            current = GET_CURRENT_FAIL();
            if (current == '/') {
                LEX_BUMP();
                current = GET_CURRENT_FAIL();
                if (current == '*') {
                    nesting_levels++;
                }
            } else if (current == '*') {
                LEX_BUMP();
                current = GET_CURRENT_FAIL();
                if (current == '/') {
                    if (nesting_levels == 0) {
                        break;
                    }
                    nesting_levels--;
                }
            }
        }
    }
    NOMATCH;
}

bool lexer_process_whitespaces(LexState *lexer) {
    uchar current;
    do {
        current = GET_CURRENT();
        IF_INPUT_EMPTY {
            SUCCESS(EOS);
        }

        if (!IS_WHITESPACE(current)) {
            break;
        }
        if (LINEBREAK(current)) {
            lexer_linebreak_add(lexer);
        }
        LEX_BUMP();
    } while (true);
    NOMATCH;
}

bool lexer_lex_char_literal(LexState *lexer) {
    uchar current = GET_CURRENT();

    if (current == 'b') {
        LEX_BUMP();
    }
    
    if (GET_CURRENT_FAIL() == '\'') {
        LEX_BUMP();
    } else {
        NOMATCH;
    }

    current = GET_CURRENT_FAIL();
    if (current == '\\') {
        LEX_BUMP();
    } else if (current == '\'') {
        FAIL(EmptyCharLiteral);
    } else {
        lexer->current_data.character = current;
        goto end;
    }
    current = GET_CURRENT_FAIL();
    // TODO: unescape current into its acutal form
    lexer->current_data.character = current;

end:
{
    LEX_BUMP();
    if (GET_CURRENT_FAIL() != '\'') {
        if (_bump_until_eol_eof(lexer, '\'')) {
            // we've found a closing ' character
            // => fail with MutliCharCharLiteral
            FAIL(MulitCharCharLiteral);
        }
        // no literal clsoig character found
        // => fail with UnclosedCharLiteral
        FAIL(UnclosedCharLiteral);
    }
    SUCCESS(CHAR);
}
}

bool lexer_lex_string_literal(LexState *lexer) {
    uchar current = GET_CURRENT();

    if (current == 'r' || current == 'b') {
        LEX_BUMP();
    }
    
    if (GET_CURRENT_FAIL() == '"') {
        LEX_BUMP();
    } else {
        NOMATCH;
    }

    while (true) {
        if (!_bump_until_eol_eof(lexer, '"')) {
            FAIL(UnclosedStringLiteral);
        }

        lexer_slice_window(lexer);
        // TODO: Push slice to current_data.string

        char last = utf8_slice_get_last_char(&lexer->window, &global_error);
        if (last != '\\') {
            break;
        }
        lexer_save_window(lexer);
    }

    // Todo: Iterate through string and unescape characters

    SUCCESS(STRING);
}

void lexer_lex(LexState *lexer) {
    lexer_save_window(lexer);
    TRY(lexer_process_whitespaces(lexer));
    uchar current = GET_CURRENT();
    lexer_save_window(lexer);

    if (current == '/') {
        TRY(lexer_lex_comment(lexer));
    }

    if (current == '\'' || current == 'b') {
        TRY(lexer_lex_char_literal(lexer));
    }

    if (current == '"' || current == 'r' || current == 'b') {
        TRY(lexer_lex_string_literal(lexer));
    }

    // if (current == '.' || CHR_IN_RANGE(current, '0', '9')) {
    //     TRY(lexer_lex_number_literal(lexer));
    // }

    // if (CHR_IN_RANGE(current, 'A', 'Z') || 
    //         CHR_IN_RANGE(current, 'a', 'z') ||
    //         current == '_' ||
    //         current == '$') {
    //     TRY(lexer_lex_identifier_or_keyword(lexer));
    // }

    lexer->token = ERROR;
    lexer->error = UnexpectedEOF;
    lexer_slice_window(lexer);
}

