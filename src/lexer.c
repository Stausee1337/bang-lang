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
    if (expr < Unmatched) { goto defer; }\
    restore(lexer);

#define FAIL(x_error) \
    ls->token = ERROR; \
    ls->error = (x_error); \
    return Error;

typedef struct {
    size_t token_start;
    Lex_Pos token_start_pos;
} State_Dump;

static
Char_Result lookahead(Lex_State *ls) {
    if (ls->input_pos + 1 == ls->input.count - 1) {
        return NONE;
    }
    return SOME(ls->input.data[ls->input_pos+1]);
}

static
char current(Lex_State *ls) {
    return SV_AT(&ls->input, ls->input_pos);
}

static
bool is_whitespace(Lex_State *ls) {
    return current(ls) == ' ';
}

static
int is_newline(Lex_State *ls) {
    if (current(ls) == '\n' && IS_SOME(lookahead(ls), '\r')) {
        return 2;
    }
    return current(ls) == '\n';
}

static
bool is_eof(Lex_State *ls) {
    return ls->token == EOS;
}

static
void bump(Lex_State *ls) {
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
        ls->token = EOS;
    }
}

static
void save(Lex_State *ls) {
    ls->token_start = ls->input_pos;
    ls->token_start_pos = ls->file_pos;
}

static
State_Dump dump(Lex_State *ls) {
    State_Dump sd;
    sd.token_start = ls->token_start;
    sd.token_start_pos = ls->token_start_pos;
    return sd;
}

static
void load(Lex_State *ls, State_Dump sd) {
    ls->token_start = sd.token_start;
    ls->token_start_pos = sd.token_start_pos;
}

static
void restore(Lex_State *ls) {
    ls->input_pos = ls->token_start;
    ls->file_pos = ls->token_start_pos;
}

static
void create_window(Lex_State *ls) {
    size_t count = ls->input_pos - ls->token_start;
    ls->window.data = (ls->input.data + ls->token_start);
    ls->window.count = count;
}

static
void finish(Lex_State *ls) {
    ls->token_end_pos = ls->file_pos;
    ls->token_end = ls->input_pos;
    create_window(ls);
}

static
Lex_Token token_for_type(Lex_State *ls) {
#define BASE_TOKEN                  \
(Lex_TokenBase){                    \
    .type = ls->token,              \
    .window = ls->window,           \
    .start = ls->token_start_pos,   \
    .end = ls->token_end_pos        \
};
#define RETURN(val) return *(Lex_Token*)&val;

    switch (ls->token) {
        case NUMBER:
        {
            Lex_TokenBase bt = BASE_TOKEN;
            Lex_TokenNumber et = {
                .base = bt,
                .data = ls->number,
                .nclass = ls->number_class
            };
            RETURN(et);
        }
        case DIRECTIVE:
        {
            Lex_TokenBase bt = BASE_TOKEN;
            Lex_TokenDirective dt = {
                .base = bt,
                .directive = ls->directive
            };
            RETURN(dt);
        }
        case KEYWORD:
        {
            Lex_TokenBase bt = BASE_TOKEN;
            Lex_TokenKeyword kt = {
                .base = bt,
                .keyword = ls->keyword
            };
            RETURN(kt);
        }
        case ERROR:
        {
            Lex_TokenBase bt = BASE_TOKEN;
            Lex_TokenError et = {
                .base = bt,
                .error = ls->error
            };
            RETURN(et);
        }
        default:
        {
            Lex_TokenBase tt = BASE_TOKEN;
            RETURN(tt);
        }
    }

#undef RETURN
#undef BASE_TOKEN
}

static
Consume_Result consume_singleline_comment(Lex_State *ls) {
    while (!is_newline(ls)) {
        bump(ls);
        if (is_eof(ls)) {
            break;
        }
    }
    bump(ls);
    ls->token = COMMENT;
    return Matched;
}

static
Consume_Result consume_multiline_comment(Lex_State *ls) {
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
    ls->token = COMMENT;
    return Matched;
}

static
Consume_Result consume_comment(Lex_State *ls) {
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
Consume_Result consume_identifier(Lex_State *ls, bool simple) {
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
                ls->directive = d; 
                ls->token = DIRECTIVE;
                return Matched;
            }
            FAIL(UnknownDirective);
        } else if (!sv_startswith(identifier, '$')) {
            // check keyword
            Lex_Keyword k;
            if (_match_keyword(identifier, &k)) {
                ls->keyword = k; 
                ls->token = KEYWORD;
                return Matched;
            }
        }
    }
end:
    ls->token = IDENT;
    return Matched;
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
bool _end_parse_number(Lex_State *ls, int base, size_t number_end_pos, Lex_NumberClass class) {
    ls->number_class = class;

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
    if (IS_FLOAT_CLASS(class)) {
        ls->number.floating = strtod(number_str, &endptr);
    } else {
        ls->number.integer = strtol(number_str, &endptr, base);
    }


    free(number_str);
    return true;
}

static
bool _check_multiple_dots_end(Lex_State *ls) {
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
Consume_Result consume_number_literal(Lex_State *ls) {
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
            ls->token = NUMBER;                                                                                \
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
            ls->token = NUMBER;
            return Matched;
        }
        if (_check_multiple_dots_end(ls)) {
            if (found == 0 && base == 10) {
                // we haven't matched anything yet
                return Unmatched;
            }
            FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, Nc_Number);
            ls->token = NUMBER;
            return Matched;
        }
        is_float = true;
        bump(ls);

        MULTIPLE_DOTS_END;
    }


    if (is_eof(ls)) {
        FAIL_COMMON_FLOAT_ERRORS;
        FAIL_BASE_UNSUPPORTED_CHR(ls, base, ls->input_pos, is_float ? Nc_FloatingPointNumber : Nc_Number);
        ls->token = NUMBER;
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
        ls->token = NUMBER;
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
        printf("Suffix: " SV_FMT "\n", SV_ARG(suffix));
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

    ls->token = NUMBER;
    return Matched;

#undef ITER_DIGITS
#undef MULTIPLE_DOTS_END 
#undef FAIL_COMMON_FLOAT_ERRORS
#undef FAIL_BASE_UNSUPPORTED_CHR
}

static
Consume_Result consume_punctuators(Lex_State *ls) {
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
    int type = 0;
    memcpy(&type, string, i);

    ls->token = type;
    for (int j = 0; j < i; j++)
        bump(ls);
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
Consume_Result consume_char_literal(Lex_State *ls) {
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
        ls->token = CHAR;
        ls->char_literal = c;
        return Matched;
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
Consume_Result consume_string_literal(Lex_State *ls) {
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

    ls->token = STRING;
    return Matched;
}

static
Consume_Result consume_note(Lex_State *ls) {
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

    ls->token = NOTE_STR;
    return Matched;
}

void lexer_init(Lex_State *lexer, String_View sv) {
    lexer->input = sv;
    lexer->file_pos.col = 1;
    lexer->file_pos.row = 1;
}

Lex_Token lexer_lex(Lex_State *lexer) {
    if (lexer->input_pos == lexer->input.count) {
        lexer->token = EOS; 
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
        finish(lexer);
        return token_for_type(lexer);
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


    lexer->token = ERROR;
    lexer->error = UnexpectedCharacter;
    bump(lexer);
defer:
{
    finish(lexer);
    return token_for_type(lexer);
}
}

void lexer_print_pos(String_Builder *sb, Lex_Pos pos) {
    char buffer[32];
    da_append(sb, '[');
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d", pos.row);
    sb_append_cstr(sb, buffer);

    da_append(sb, ':');

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d", pos.col);
    sb_append_cstr(sb, buffer);
    da_append(sb, ']');
}

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

void lexer_print_token(String_Builder *sb, Lex_Token *token) {
 
    sb_append_cstr(sb, "Token { ");
    // .type
    sb_append_cstr(sb, "type = ");
    Lex_TokenType typ = lex_tok_typ(token);
    if (typ < 0x80) {
        da_append(sb, (char)typ);
    } else if (typ <= 0xff) {
        sb_append_cstr(sb, lexer_token_names[typ - 0x80]);
    } else if (typ <= 0xffff) {
        da_append_many(sb, (char*)&typ, 2);
    } else if (typ <= 0xffffff) {
        da_append_many(sb, (char*)&typ, 3);
    } else if (typ <= 0xffffffff) {
        da_append_many(sb, (char*)&typ, 4);
    }

    // .length
    /*sb_append_cstr(sb, ", length = ");
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%zu", lex_tok_sv(token).count);
    sb_append_cstr(sb, buffer);*/

    sb_append_cstr(sb, ", start = ");
    lexer_print_pos(sb, lex_tok_start(token));

    sb_append_cstr(sb, ", end = ");
    lexer_print_pos(sb, lex_tok_end(token));

    switch (lex_tok_typ(token)) {
        case NUMBER: 
        {
            Lex_TokenNumber *num = lex_tok_cast(Lex_TokenNumber, token);
            sb_append_cstr(sb, ", class = ");
            sb_append_cstr(sb, number_class_to_string(num->nclass));
            sb_append_cstr(sb, ", number = ");
            _sprintf_number(num->data, num->nclass, sb);
        } 
        break;
        case DIRECTIVE:
        {
            Lex_TokenDirective *dir = lex_tok_cast(Lex_TokenDirective, token);
            sb_append_cstr(sb, ", directive = ");
            sb_append_cstr(sb, directive_to_string(dir->directive));
        }
        break;
        case KEYWORD:
        {
            Lex_TokenKeyword *key = lex_tok_cast(Lex_TokenKeyword, token);
            sb_append_cstr(sb, ", keyword = ");
            sb_append_cstr(sb, keyword_to_string(key->keyword));
        }
        break;
        case ERROR: 
        {
            sb_append_cstr(sb, ", error = ");
            sb_append_cstr(sb, lexer_error_names[lex_tok_err(token)]);
        } 
        break;
        default:
        {
            if (!lex_tok_is_punct(token)) {            
                sb_append_cstr(sb, ", slice = '");
                da_append_many(sb, lex_tok_sv(token).data, lex_tok_sv(token).count);
                da_append(sb, '\'');
            }
        }
        break;
    }

    sb_append_cstr(sb, " }");

}

