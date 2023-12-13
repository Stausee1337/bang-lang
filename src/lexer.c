#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"

#define LEXERC_H_IMPLEMENTATION
#define OPERATORS_H_IMPLEMENTATION
#include "lexer.h"
#include "lexer_names.h"
#include "strings.h"

typedef struct {
    bool is_some;
    char value;
} Char_Result;

#define NONE \
    (Char_Result){ .is_some = false, .value = '\0' }
#define SOME(c) \
    (Char_Result){ .is_some = true, .value = (c) }
#define IS_SOME(r, c) \
    ((r).is_some && (r).value == (c))

typedef enum {
    Error = -1,
    Matched,
    Unmatched
} Consume_Result;

#define IS_LETTER(chr) \
    ((chr) >= 'a' && (chr) <= 'z') || ((chr) >= 'A' && (chr) <= 'Z')

#define IS_DIGIT(chr) \
    ((chr) >= '0' && (chr) <= '9')

#define IS_HEX(chr) \
    ((chr >= 'a' && chr <= 'f') || (chr >= 'A' && chr <= 'F'))

#define CHR_RANGE(chr, low, hi) \
    ((chr) >= low && (chr) <= hi)

// !%&()*+,-./:;<=>?[]^{|}~
#define IS_PUNCTUATOR(chr) \
    (chr == '!' || \
    CHR_RANGE(chr, '%', '&') || \
    CHR_RANGE(chr, '(', '/') || \
    CHR_RANGE(chr, ':', ';') || \
    CHR_RANGE(chr, '<', '?') || \
    chr == '[' || \
    CHR_RANGE(chr, ']', '^') || \
    CHR_RANGE(chr, '{', '~'))
 
#define CONTINUE_UNMATCHED(expr) \
    if ((expr) < Unmatched) { return; }\
    restore(lexer);

#define TK_SPAN() finish(ls)

#define MATCHED1(name) \
    ls->token = (Lex_Token) {   \
        .kind = Tk_##name,     \
        .span = TK_SPAN(),      \
    };                          \
    return Matched;

#define MATCHED(name, ...) \
    ls->token = (Lex_Token) {           \
        .kind = Tk_##name,              \
        .span = TK_SPAN(),              \
        .Tk_##name = (Lex_Token##name) {\
            __VA_ARGS__                 \
        }                               \
    };                                  \
    return Matched;

#define FAIL(x_error) \
    ls->token = (Lex_Token) {          \
        .kind = Tk_Error,              \
        .span = TK_SPAN(),             \
        .Tk_Error = (Lex_TokenError) { \
            .error = x_error           \
        }                              \
    };                                 \
    return Error;

typedef struct {
    size_t token_start;
    Lex_Pos token_start_pos;
} State_Dump;

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
} Lexer_State;

static
Char_Result lookahead(Lexer_State *ls) {
    if (ls->input_pos + 1 == ls->input.count - 1) {
        return NONE;
    }
    return SOME(ls->input.data[ls->input_pos+1]);
}

static
char current(Lexer_State *ls) {
    return SV_AT(&ls->input, ls->input_pos);
}

static
bool is_whitespace(Lexer_State *ls) {
    return current(ls) == ' ';
}

static
int is_newline(Lexer_State *ls) {
    if (current(ls) == '\n' && IS_SOME(lookahead(ls), '\r')) {
        return 2;
    }
    return current(ls) == '\n';
}

static
bool is_eof(Lexer_State *ls) {
    return ls->token.kind == Tk_EOF;
}

static
void bump(Lexer_State *ls) {
    if (is_eof(ls)) return; 

    size_t n_nl = 0;
    if ((n_nl = is_newline(ls))) { 
        ls->input_pos += n_nl;
        ls->file_pos.col = 1;
        ls->file_pos.row += 1;
    } else {
        ls->input_pos++;
        ls->file_pos.col += 1;
    }

    if (ls->input_pos == ls->input.count) {
        ls->token = (Lex_Token) {
            .kind = Tk_EOF,
            .span = {{0}}
        };
    }
}

static
void save(Lexer_State *ls) {
    ls->token_start = ls->input_pos;
    ls->token_start_pos = ls->file_pos;
}

static
State_Dump dump(Lexer_State *ls) {
    State_Dump sd;
    sd.token_start = ls->token_start;
    sd.token_start_pos = ls->token_start_pos;
    return sd;
}

static
void load(Lexer_State *ls, State_Dump sd) {
    ls->token_start = sd.token_start;
    ls->token_start_pos = sd.token_start_pos;
}

static
void restore(Lexer_State *ls) {
    ls->input_pos = ls->token_start;
    ls->file_pos = ls->token_start_pos;
}

static
void create_window(Lexer_State *ls) {
    size_t count = ls->input_pos - ls->token_start;
    ls->window.data = (ls->input.data + ls->token_start);
    ls->window.count = count;
}

static
Lex_Span finish(Lexer_State *ls) {
    ls->token_end_pos = (Lex_Pos) {
        .row = ls->file_pos.row,
        .col = ls->file_pos.col - 1
    };
    ls->token_end = ls->input_pos;
    create_window(ls);

    return (Lex_Span) {
        .filename = ls->filename,
        .start = ls->token_start_pos,
        .end = ls->token_end_pos
    };
}

static
Consume_Result consume_singleline_comment(Lexer_State *ls) {
    while (!is_newline(ls)) {
        bump(ls);
        if (is_eof(ls)) {
            break;
        }
    }
    bump(ls);
    MATCHED1(Comment);
}

static
Consume_Result consume_multiline_comment(Lexer_State *ls) {
    size_t level = 0;
    while (true) {
        if (current(ls) == '*' && IS_SOME(lookahead(ls), '/')) {
            bump(ls);
            if (level == 0) goto end;
            level--;
        }
        if (current(ls) == '/' && IS_SOME(lookahead(ls), '*')) {
            bump(ls);
            level++;
        }
        bump(ls);
        if (is_eof(ls)) {
            goto end;
        }
    }
end:
    bump(ls);
    MATCHED1(Comment);
}

static
Consume_Result consume_comment(Lexer_State *ls) {
    bump(ls);
    if (is_eof(ls)) {
        return Unmatched;
    }
    char curr = current(ls);
    if (curr == '/') {
        bump(ls);
        return consume_singleline_comment(ls);
    } else if (curr == '*') {
        bump(ls);
        return consume_multiline_comment(ls);
    }
    return Unmatched;
}

static
bool _match_directive(String_View sv, Lex_Directive *res) {
    char *str = malloc(sv.count + 1);

    memcpy(str, sv.data, sv.count);
    str[sv.count] = 0;

    *res = directive_resolve(str);

    free(str);
    return *res != D_Invalid;
}

static
bool _match_keyword(String_View sv, Lex_Keyword *res) {
    char *str = malloc(sv.count + 1);

    memcpy(str, sv.data, sv.count);
    str[sv.count] = 0;

    *res = keyword_resolve(str);

    free(str);
    return *res != K_Invalid;
}

static
String_Builder window_to_string(String_View window) {
    char *str = malloc(window.count);
    memcpy(str, window.data, window.count);

    return (String_Builder) {
        .items = str,
        .count = window.count,
        .capacity = window.count
    };
}

static
Consume_Result consume_identifier(Lexer_State *ls, bool simple) {
    char curr = current(ls);
    if (curr == '#') {
        bump(ls);
        if (is_eof(ls)) {
            FAIL(UnexpectedEOF);
        }
        curr = current(ls);
    }
    while (curr == '$' || curr == '_' || IS_LETTER(curr) || IS_DIGIT(curr)) {
        bump(ls);
        if (is_eof(ls)) {
            goto end;
        }
        curr = current(ls);
    }
    if (!simple) {
        create_window(ls);
        String_View identifier = ls->window;
        if (sv_startswith(identifier, '#')) {
            // check directive
            Lex_Directive d;
            if (_match_directive(sv_slice(identifier, 1, identifier.count), &d)) {
                MATCHED(Directive, .directive = d)
            }
            FAIL(UnknownDirective);
        } else if (!sv_startswith(identifier, '$')) {
            // check keyword
            Lex_Keyword k;
            if (_match_keyword(identifier, &k)) {
                MATCHED(Keyword, .keyword = k)
            }
        }
    }
end:
    MATCHED(Ident, .name = window_to_string(ls->window))
}

static
bool _is_valid_number_suffix(String_View sv) {
    return (
        sv_eq_cstr(sv, "i8") ||
        sv_eq_cstr(sv, "u8") ||
        sv_eq_cstr(sv, "i16") ||
        sv_eq_cstr(sv, "u16") ||
        sv_eq_cstr(sv, "i32") ||
        sv_eq_cstr(sv, "u32") ||
        sv_eq_cstr(sv, "i64") ||
        sv_eq_cstr(sv, "u64") ||
        sv_eq_cstr(sv, "isize") ||
        sv_eq_cstr(sv, "usize") ||
        sv_eq_cstr(sv, "f32") ||
        sv_eq_cstr(sv, "f64")
    );
}

static
bool _is_float_suffix(String_View sv) {
    return sv_eq_cstr(sv, "f32") || sv_eq_cstr(sv, "f64");
}

static
bool _end_parse_number(Lexer_State *ls, int base, size_t number_end_pos, Lex_NumberClass class) {
    create_window(ls);
    int start = 0;
    if (base != 10) {
        start = 2;
    }

    size_t number_len = number_end_pos - ls->token_start;
    String_View number_sv = sv_slice(ls->window, start, number_len);

#define CHR_IN_RANGE(chr, base) \
    ((chr) >= '0' && (chr) <= ('0' + (base - 1)))

    if (base == 2 || base == 8) {
        sv_for_each(&number_sv, chr, {
            if (!CHR_IN_RANGE(chr, base)) {
                return false;
            }
        });
    }
#undef CHR_IN_RANGE

    char* number_str = malloc(number_sv.count + 1);
    memset(number_str, 0, number_sv.count + 1);
    memcpy(number_str, number_sv.data, number_sv.count);

    char *endptr;
    union _64_bit_number number;
    if (IS_FLOAT_CLASS(class)) {
        number.floating = strtod(number_str, &endptr);
    } else {
        number.integer = strtol(number_str, &endptr, base);
    }

    ls->token = (Lex_Token) {
        .kind = Tk_Number,
        .span = TK_SPAN(),
        .Tk_Number = (Lex_TokenNumber) {
            .number = number,
            .nclass = class
        }
    };

    free(number_str);
    return true;
}

static
bool _check_multiple_dots_end(Lexer_State *ls) {
    size_t remaing_count = ls->input.count - ls->input_pos;
    if (remaing_count >= 2) {
        // check for multi dot syntax
        const char *string = ls->input.data + ls->input_pos;
        int type = 0;
        memcpy(&type, string, 3);
        if (type == DOUBLE_DOT) {
            return true;
        }
    }
    return false;
}

static
Consume_Result consume_number_literal(Lexer_State *ls) {
    Char_Result next = lookahead(ls);
    bool literal_lowercase = IS_SOME(next, 'b') || IS_SOME(next, 'o') || IS_SOME(next, 'x');
    bool literal_uppercase = IS_SOME(next, 'B') || IS_SOME(next, 'O') || IS_SOME(next, 'X');
    int base = 10;
    if (current(ls) == '0' && (literal_lowercase || literal_uppercase)) {
        bump(ls);
        switch (current(ls)) {
            case 'B':
            case 'b':
                base = 2;
                break;
            case 'O':
            case 'o':
                base = 8;
                break;
            case 'X':
            case 'x':
                base = 16;
                break;
            default:
                assert(false && "unreachable");
        }
        bump(ls);
        if (is_eof(ls)) {
            FAIL(UnexpectedEOF);
        }
    }

#define ITER_DIGITS(num, expr) \
do {                                                         \
    num = 0;                                                 \
    char curr = current(ls);                                 \
    while (IS_DIGIT(curr) || (expr)) { \
        num++;                                               \
        bump(ls);                                            \
        if (is_eof(ls)) {                                    \
            break;                                           \
        }                                                    \
        curr = current(ls);                                  \
    }                                                        \
} while (0);


#define MULTIPLE_DOTS_END \
while (true) {                                                                                                 \
    ITER_DIGITS(found, false);                                                                                 \
    if (!is_eof(ls) && current(ls) == '.') {                                                                   \
        if (_check_multiple_dots_end(ls)) {                                                                    \
            FAIL_COMMON_FLOAT_ERRORS;                                                                          \
            FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, is_float ? Nc_FloatingPointNumber : Nc_Number); \
            return Matched;                                                                                    \
        }                                                                                                      \
    } else {                                                                                                   \
        break;                                                                                                 \
    }                                                                                                          \
}

#define FAIL_COMMON_FLOAT_ERRORS          \
if (base != 10 && is_float) {             \
    FAIL(DifferentBaseFloatingLiteral);   \
}                                         \
if (multiple_dots_in_float) {             \
    FAIL(MultipleDotsInFloat);            \
}
#define FAIL_BASE_UNSUPPORTED_CHR(...) \
   if(!_end_parse_number(__VA_ARGS__)) { FAIL(UnsupportedDigitForBase); }

    int found;
    ITER_DIGITS(found, base == 16 && IS_HEX(curr));

    bool is_float = false;
    bool multiple_dots_in_float = false;
    if (!is_eof(ls) && current(ls) == '.') {
        next = lookahead(ls);
        if (!next.is_some || !(IS_DIGIT(next.value) || next.value == '.' || next.value == 'e' || next.value == 'E')){
            if (found == 0 && base == 10) {
                // we haven't matched anything yet
                // it's probably just the `.` token
                return Unmatched;
            }
            bump(ls);
            FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, Nc_FloatingPointNumber);
            return Matched;
        }
        if (_check_multiple_dots_end(ls)) {
            if (found == 0 && base == 10) {
                // we haven't matched anything yet
                return Unmatched;
            }
            FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, Nc_Number);
            return Matched;
        }
        is_float = true;
        bump(ls);

        MULTIPLE_DOTS_END;
    }


    if (is_eof(ls)) {
        FAIL_COMMON_FLOAT_ERRORS;
        FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, is_float ? Nc_FloatingPointNumber : Nc_Number);
        return Matched;
    }

    char curr = current(ls);
    if (curr == 'e' || curr == 'E') {
        next = lookahead(ls);
        if (!next.is_some || !(IS_DIGIT(next.value) || next.value == '.' || next.value == '+' || next.value == '-')) {
            bump(ls);
            FAIL(SientificFloatWithoutExponent);
        }
        is_float = true;
        bump(ls);
        curr = current(ls);
        if (curr == '+' || curr == '-') {
            bump(ls);
        }
        MULTIPLE_DOTS_END;
    }

    if (is_eof(ls)) {
        FAIL_COMMON_FLOAT_ERRORS;
        FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, is_float ? Nc_FloatingPointNumber : Nc_Number);
        return Matched;
    }

    size_t number_end_pos = ls->input_pos;
    curr = current(ls);
    String_View suffix;
    int suffix_exists = -1;
    if (curr == 'u' || curr == 'i' || curr == 'f') {
        State_Dump state = dump(ls);
        save(ls);
        consume_identifier(ls, /* simple */ true);
        create_window(ls);
        suffix = ls->window;
        suffix_exists = _is_valid_number_suffix(suffix);
        if (suffix_exists == 0) {
            restore(ls);
        }
        load(ls, state);
    }

    bool has_float_suffix = false;
    if (suffix_exists == 1) {
        has_float_suffix = _is_float_suffix(suffix);
    }

    FAIL_COMMON_FLOAT_ERRORS;

    if (base != 10 && has_float_suffix) {
        FAIL(DifferentBaseFloatingLiteral);
    }

    if (is_float && suffix_exists == 1 && !has_float_suffix) {
        FAIL(InvalidSuffixForFloat);
    }

    Lex_NumberClass class;
    if (suffix_exists == 1) {
        if (sv_eq_cstr(suffix, "i8")) {
            class = Nc_i8; 
        } else if (sv_eq_cstr(suffix, "u8")) {
            class = Nc_u8;
        } else if (sv_eq_cstr(suffix, "i16")) {
            class = Nc_i16;
        } else if (sv_eq_cstr(suffix, "u16")) {
            class = Nc_u16;
        } else if (sv_eq_cstr(suffix, "i32")) {
            class = Nc_i32;
        } else if (sv_eq_cstr(suffix, "u32")) {
            class = Nc_u32;
        } else if (sv_eq_cstr(suffix, "i64")) {
            class = Nc_i64;
        } else if (sv_eq_cstr(suffix, "u64")) {
            class = Nc_u64;
        } else if (sv_eq_cstr(suffix, "isize")) {
            class = Nc_isize;
        } else if (sv_eq_cstr(suffix, "usize")) {
            class = Nc_usize;
        } else if (sv_eq_cstr(suffix, "f32")) {
            class = Nc_f32;
        } else if (sv_eq_cstr(suffix, "f64")) {
            class = Nc_f64;
        } else {
            assert(false && "Unreachable, we dont filter number classes correctly");
        }
    } else {
        if (is_float) {
            class = Nc_FloatingPointNumber;
        } else {
            class = Nc_Number;
        }
    }

    FAIL_BASE_UNSUPPORTED_CHR(ls, base, number_end_pos, class);
    return Matched;

#undef ITER_DIGITS
#undef MULTIPLE_DOTS_END 
#undef FAIL_COMMON_FLOAT_ERRORS
#undef FAIL_BASE_UNSUPPORTED_CHR
}

static
Consume_Result consume_punctuators(Lexer_State *ls) {
    // get the next 4 chars as int
    size_t remaing_count = ls->input.count - ls->input_pos;
    const char *string = ls->input.data + ls->input_pos;
    if (remaing_count > 3) {
        remaing_count = 3;
    }

    int i = remaing_count;
    bool found = false;
    char punct[3] = {0};

    for (; i > 0; i--) {
        memset(punct, 0, 3);
        memcpy(punct, string, i);

        if (check_is_punctuator(punct)) {
            found = true;
            break;
        }
    }
    if (!found) {
        bump(ls);
        FAIL(UnknownPunctuator);
    }
    int kind = 0;
    memcpy(&kind, string, i);

    for (int j = 0; j < i; j++)
        bump(ls);
    ls->token = (Lex_Token) {
        .kind = kind,
        .span = TK_SPAN()
    };
    return Matched;
}

static
int _unescape_char(char after_esc, char closing, char *res) {
    if (after_esc == closing) {
        *res = closing;
        return true;
    }
    switch (after_esc) {
        case 'n':
            *res = '\n';
            break;
        case '\\':
            *res = '\\';
            break;
        default:
            return false;
    }
    return true;
}

static
Consume_Result consume_char_literal(Lexer_State *ls) {
    char curr = current(ls);

    if (curr == 'b') {
        bump(ls);
        if (is_eof(ls)) {
            return Unmatched;
        }
        curr = current(ls);
    }

    if (curr != '\'') {
        return Unmatched;
    }
    bump(ls);

    if (is_eof(ls)) {
        FAIL(UnexpectedEOF);
    }

    bool invalid_escape = false;
    curr = current(ls);
    char c;
    if (curr == '\'') {
        bump(ls);
        FAIL(EmptyCharLiteral);
    } else if (curr == '\\') {  
        bump(ls);
        if (is_eof(ls)) {
            FAIL(UnexpectedEOF);
        }
        if (!_unescape_char(current(ls), '\'', &c)) {
            invalid_escape = true;
        }
    } else {
        c = current(ls);
    }

    bump(ls);
    if (is_eof(ls)) {
        FAIL(UnexpectedEOF);
    }

    if (current(ls) == '\'') {
        bump(ls);
        if (invalid_escape) {
            FAIL(InvalidEscape);
        }
        MATCHED(Char, .wchar = c);
    }

    while (!is_newline(ls)) {
        bump(ls);
        if (is_eof(ls) || current(ls) == '\'') {
            break;
        }
    }

    if (!is_eof(ls) && current(ls) == '\'') {
        bump(ls);
        FAIL(MulitCharCharLiteral);
    }
    FAIL(UnclosedCharLiteral);
}

static
Consume_Result consume_string_literal(Lexer_State *ls) {
    char curr = current(ls);

    if (curr == 'b') {
        bump(ls);
        if (is_eof(ls)) {
            return Unmatched;
        }
        curr = current(ls);
    }

    if (curr != '"') {
        return Unmatched;
    }    
    bump(ls);

    if (is_eof(ls)) {
        FAIL(UnexpectedEOF);
    }

    bool invalid_escape = false;
    curr = current(ls);
    while (curr != '"') {
        bump(ls);
        if (is_eof(ls) || is_newline(ls)) {
            break;
        }
        curr = current(ls);
        if (curr == '\\') {
            bump(ls);
            if (is_eof(ls)) {
                FAIL(UnexpectedEOF);
            }
            char res;
            if (!_unescape_char(current(ls), '"', &res)) {
                invalid_escape |= true;
            }
        }
    }

    if (is_eof(ls) || is_newline(ls)) {
        FAIL(UnclosedStringLiteral);
    }
    bump(ls);

    if (invalid_escape) {
        FAIL(InvalidEscape);
    }

    MATCHED(String, .string = window_to_string(ls->window));
}

static
Consume_Result consume_note(Lexer_State *ls) {
    bump(ls);
    if (is_eof(ls)) {
        FAIL(InvalidZeroSizeNote);
    }
    char curr = current(ls);
    while (true) {
        if (is_eof(ls) || is_newline(ls) || curr == '@') {
            break;
        }
        bump(ls);
        curr = current(ls);
    }
    if (ls->input_pos - ls->token_start < 2) {
        FAIL(InvalidZeroSizeNote);
    }

    MATCHED(Note, .note = window_to_string(ls->window));
}

static
Lexer_State lexer_init(String_View filename, String_View input) {
    return (Lexer_State) {
        .filename = filename,
        .input = input,
        .file_pos.col = 1,
        .file_pos.row = 1
    };
}

static
void lexer_next(Lexer_State *lexer) {
    if (lexer->input_pos == lexer->input.count) {
        goto eof;
    }
    while (true) {
        if (is_eof(lexer)) {
            break;
        }
        if (!is_whitespace(lexer) && !is_newline(lexer)) {
            break;
        }
        bump(lexer);
    }
    if (is_eof(lexer)) {
eof:
        lexer->token_start = lexer->input_pos;
        lexer->token_start_pos = lexer->file_pos;
        lexer->token = (Lex_Token) {
            .kind = Tk_EOF,
            .span = (Lex_Span) {
                .filename = lexer->filename,
                .start = lexer->token_start_pos,
                .end = lexer->token_end_pos
            }
        };
        return;
    }
    save(lexer);

    char curr = current(lexer);
    if (curr == '/') {
        CONTINUE_UNMATCHED(consume_comment(lexer));
    }

    if (curr == '\'' || curr == 'b') {
        CONTINUE_UNMATCHED(consume_char_literal(lexer));
    }

    if (curr == '"' || curr == 'b') {
        CONTINUE_UNMATCHED(consume_string_literal(lexer));
    }

    if (curr == '$' || curr == '_' || curr == '#' || IS_LETTER(curr)) {
        CONTINUE_UNMATCHED(consume_identifier(lexer, /* simple */ false));
    }

    if (IS_DIGIT(curr) || curr == '.') {
        CONTINUE_UNMATCHED(consume_number_literal(lexer));
    }

    if (IS_PUNCTUATOR(curr)) {
        CONTINUE_UNMATCHED(consume_punctuators(lexer));
    }

    if (curr == '@') {
        CONTINUE_UNMATCHED(consume_note(lexer));
    }


    lexer->token = (Lex_Token) {
        .kind = Tk_Error,
        .span = finish(lexer),
        .Tk_Error = (Lex_TokenError) {
            .error = UnexpectedCharacter 
        }
    };
    bump(lexer);
}

Lex_Delimiter _delimiter_from_char(char delim) {
    switch (delim) {
        case '(':
        case ')':
            return Dl_Paren;
        case '{':
        case '}':
            return Dl_Brace;
        case '[':
        case ']':
            return Dl_Bracket;
    }
    assert(false && "char is not a delimiter");
}

#define ERROR_SUCCESS (Lex_Error)-1
Lex_TokenStream _recursively_get_stream(Lexer_State *lexer, Lex_Delimiter delimiter, Lex_StreamError *error) {
    Lex_TokenStream stream = {0};

    while (true) {
        if (is_eof(lexer)) {
            goto end;
        }
        lexer_next(lexer);
        Lex_Token token = lexer->token;
        Lex_TokenTree tree = {0};

        switch ((int)token.kind) {
            case '(': 
            case '{':
            case '[':
            {
                Lex_StreamError inner_error = { .type = ERROR_SUCCESS, .span = token.span };
                Lex_Delimiter delim = _delimiter_from_char((char)token.kind);
                Lex_TokenStream recursed = _recursively_get_stream(lexer, delim, &inner_error);
                if (inner_error.type != ERROR_SUCCESS) {
                    *error = inner_error;
                    goto error;
                }

                tree = (Lex_TokenTree) {
                    .type = Tt_Delimited,
                    .Tt_Delimited = (Lex_Delimited) {
                        .delimiter = delim,
                        .stream = recursed,
                        .span = {
                            .open = token.span,
                            .close = lexer->token.span
                        }
                    }
                };
            } break;
            case ')': 
            case '}':
            case ']':
            {
                Lex_Delimiter delim = _delimiter_from_char((char)token.kind);
                if (delimiter != delim) {
                    if (delimiter == 0) {
                        *error = (Lex_StreamError) {
                            .type = UnexpectedDelimiter,
                            .span = token.span
                        };
                    } else {
                    *error = (Lex_StreamError) {
                        .type = MismatchedDelimiter,
                        .span = {
                            .start = error->span.start,
                            .end = token.span.start
                        }
                    };
                    }
                    goto error;
                }
                // assert(delimiter == delim && "TODO: ERROR: unexpected closing delimiters");
                goto end;
            } break;
            case Tk_EOF:
            {
                // assert(delimiter == 0 && "TODO: ERROR: missing closing delimiter");
                if (delimiter != 0) {
                    error->type = MissingDelimiter;
                    goto error;
                }
                tree = (Lex_TokenTree) {
                    .type = Tt_Token,
                    .Tt_Token = token
                };
            } break;
            default:
            {
                tree = (Lex_TokenTree) {
                    .type = Tt_Token,
                    .Tt_Token = token
                };
            } break;
        }

        da_append(&stream, tree);
    }
end:
    return stream;
error:
{
    lexer_token_stream_free(&stream);
    return stream;
}
}

Lex_TokenizeResult lexer_tokenize_source(String_View filename, String_View content, bool *success) {
    Lexer_State lexer = lexer_init(filename, content);

    Lex_StreamError error = { .type = ERROR_SUCCESS, .span = {{0}} };
    Lex_TokenStream stream =  _recursively_get_stream(&lexer, /* delimiter */ 0, &error);

    if (error.type != ERROR_SUCCESS) {
        *success = false;
        return (Lex_TokenizeResult) {
            .error = error
        };
    }
    *success = true;
    return (Lex_TokenizeResult) {
        .stream = stream
    };
}

void lexer_token_free(Lex_Token token) {
    switch (token.kind) {
        case Tk_String:
        case Tk_Note:
        {
            String_Builder string = token.kind == Tk_Note ? token.Tk_Note.note : token.Tk_String.string;
            free(string.items);
        } break;
        case Tk_Ident:
        {
            free(token.Tk_Ident.name.items);
        } break;
        default: break;
    }
}

void lexer_token_stream_free(Lex_TokenStream *stream) {
    for (size_t i = 0; i < stream->count; i++) {
        Lex_TokenTree tree = stream->items[i];
        switch (tree.type) {
            case Tt_Delimited:
                lexer_token_stream_free(&tree.Tt_Delimited.stream);
                break;
            case Tt_Token:
                lexer_token_free(tree.Tt_Token);
                break;
        }
    }
    free(stream->items);
    stream->count = 0;
    stream->capacity = 0;
}

void lexer_print_pos(String_Builder *sb, Lex_Pos pos) {
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%zu", pos.row);
    sb_append_cstr(sb, buffer);

    da_append(sb, ':');

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%zu", pos.col);
    sb_append_cstr(sb, buffer);
}

void lexer_print_span(String_Builder *sb, Lex_Span span) {
    da_append(sb, '[');
    lexer_print_pos(sb, span.start);
    sb_append_cstr(sb, "..");
    lexer_print_pos(sb, span.end);
    da_append(sb, ']');
}

static
void _sprintf_number(union _64_bit_number number, Lex_NumberClass class, String_Builder *sb) {
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    if (IS_FLOAT_CLASS(class)) {
        sprintf(buffer, "%f", number.floating);
    } else {
        sprintf(buffer, "%zu", number.integer);
    }
    sb_append_cstr(sb, buffer);
}

void lexer_print_delimited(String_Builder *sb, Lex_Delimited *token) {
    sb_append_cstr(sb, "Delimited { ");
    sb_append_cstr(sb, "type = ");
    switch (token->delimiter) {
        case Dl_Paren: 
            sb_append_cstr(sb, "Paren, ");
            break;
        case Dl_Brace: 
            sb_append_cstr(sb, "Brace, ");
            break;
        case Dl_Bracket: 
            sb_append_cstr(sb, "Bracket, ");
            break;
    }

    sb_append_cstr(sb, "open = ");
    lexer_print_span(sb, token->span.open);
    sb_append_cstr(sb, ", close = ");
    lexer_print_span(sb, token->span.close);
    sb_append_cstr(sb, " }");
}

inline static
const char *tokenkind_to_string(Lex_TokenKind in) {
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
const char *error_to_string(Lex_Error in) {
    static const char *lexer_error_names[] = {
#define _E(name) #name,
        ENUMERATE_LEXER_ERRORS
#undef _E
    };
    assert(in < NumberOfErrors);

    return lexer_error_names[in];
}

void lexer_print_token(String_Builder *sb, Lex_Token *token) { 
    sb_append_cstr(sb, "Token { ");
    sb_append_cstr(sb, "type = ");
    Lex_TokenKind kind = token->kind;
    if (kind < 0x80) {
        da_append(sb, (char)kind);
    } else if (kind <= 0xff) {
        sb_append_cstr(sb, tokenkind_to_string(kind));
    } else if (kind <= 0xffff) {
        da_append_many(sb, (char*)&kind, 2);
    } else if (kind <= 0xffffff) {
        da_append_many(sb, (char*)&kind, 3);
    } else if (kind <= 0xffffffff) {
        da_append_many(sb, (char*)&kind, 4);
    }

    sb_append_cstr(sb, ", span = ");
    lexer_print_span(sb, token->span);

    switch (kind) {
        case Tk_Number: 
        {
            Lex_TokenNumber num = token->Tk_Number;
            sb_append_cstr(sb, ", class = ");
            sb_append_cstr(sb, number_class_to_string(num.nclass));
            sb_append_cstr(sb, ", number = ");
            _sprintf_number(num.number, num.nclass, sb);
        } 
        break;
        case Tk_Directive:
        {
            Lex_TokenDirective dir = token->Tk_Directive;
            sb_append_cstr(sb, ", directive = ");
            sb_append_cstr(sb, directive_to_string(dir.directive));
        }
        break;
        case Tk_Keyword:
        {
            Lex_TokenKeyword key = token->Tk_Keyword;
            sb_append_cstr(sb, ", keyword = ");
            sb_append_cstr(sb, keyword_to_string(key.keyword));
        }
        break;
        case Tk_Ident:
        {
            Lex_TokenIdent ident = token->Tk_Ident;
            sb_append_cstr(sb, ", ident = ");
            da_append_many(sb, ident.name.items, ident.name.count);
        }
        break;
        case Tk_Char:
        {
            Lex_TokenChar tchar = token->Tk_Char;
            char buffer[4];
            sprintf(buffer, "0x%x", tchar.wchar);
            sb_append_cstr(sb, ", char = ");
            sb_append_cstr(sb, buffer);
        }
        break;
        case Tk_String:
        case Tk_Note:
        {
            String_Builder string = kind == Tk_Note ? token->Tk_Note.note : token->Tk_String.string;
            sb_append_cstr(sb, ", ");
            sb_append_cstr(sb, kind == Tk_Note ? "note" : "string");
            sb_append_cstr(sb, " = ");
            da_append_many(sb, string.items, string.count);
        }
        break;
        case Tk_Error: 
        {
            Lex_TokenError err = token->Tk_Error;
            sb_append_cstr(sb, ", error = ");
            sb_append_cstr(sb, error_to_string(err.error));
        } 
        break;
        default: break;
    }

    sb_append_cstr(sb, " }");

}

void lexer_print_error(String_Builder *sb, Lex_Error *error) {
    sb_append_cstr(sb, error_to_string(*error));
}

