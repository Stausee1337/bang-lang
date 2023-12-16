// this file was generated from operators.h.templ8
#ifndef  OPERATORS_H_
#define  OPERATORS_H_

#include <assert.h>
#ifndef  OPERATORS_H_PREFIX
#define  OPERATORS_H_PREFIX
#endif //OPERATORS_H_PREFIX
#include <stdint.h>

uint64_t thirdparty_siphash24(const void *src, unsigned long src_sz, const char key[16]);
#include <stdbool.h>
#include <string.h>

#define NUM_ENTRIES_BINARY_OP 18

typedef enum {
    Bo_Invalid = -1,
    Bo_Mul,
    Bo_Div,
    Bo_Mod,
    Bo_Plus,
    Bo_Minus,
    Bo_Shl,
    Bo_Shr,
    Bo_BAnd,
    Bo_BXor,
    Bo_BOr,
    Bo_Eq,
    Bo_Ne,
    Bo_Gt,
    Bo_Ge,
    Bo_Lt,
    Bo_Le,
    Bo_And,
    Bo_Or,
    Bo_NumberOfElements
} BinaryOp;

static_assert(Bo_NumberOfElements == NUM_ENTRIES_BINARY_OP, "Number of enum BinaryOp elements changed. This file was generated by operators.h.templ8, edit this instead");

unsigned char binary_op_get_precedence(BinaryOp in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
unsigned char binary_op_get_precedence(BinaryOp in) {
    static const unsigned char bo_operator_precedences[NUM_ENTRIES_BINARY_OP] = {
        11, 11, 11, 10, 10, 9, 9, 8, 7, 6, 5, 5, 5, 5, 5, 5, 4, 3, };

    assert(in < NUM_ENTRIES_BINARY_OP);
    return bo_operator_precedences[in];
}
#endif //OPERATORS_H_IMPLEMENTATION

const char *binary_op_to_string(BinaryOp in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
const char *binary_op_to_string(BinaryOp in) {
    static const char *binary_op_str_map[NUM_ENTRIES_BINARY_OP] = {
        "Mul",
        "Div",
        "Mod",
        "Plus",
        "Minus",
        "Shl",
        "Shr",
        "BAnd",
        "BXor",
        "BOr",
        "Eq",
        "Ne",
        "Gt",
        "Ge",
        "Lt",
        "Le",
        "And",
        "Or",
    };
    assert(in < NUM_ENTRIES_BINARY_OP);

    return binary_op_str_map[in];
}
#endif //OPERATORS_H_IMPLEMENTATION

BinaryOp binary_op_resolve(uint32_t in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
BinaryOp binary_op_resolve(uint32_t in) {
    static const struct _bo_struct_tuple { uint32_t _0; BinaryOp _1; } _bo_entries[NUM_ENTRIES_BINARY_OP] = {
        { 0x0000003c, Bo_Lt },
        { 0x0000007c, Bo_BOr },
        { 0x00003c3c, Bo_Shl },
        { 0x0000002d, Bo_Minus },
        { 0x00003d3e, Bo_Ge },
        { 0x00003d3d, Bo_Eq },
        { 0x00000026, Bo_BAnd },
        { 0x00000025, Bo_Mod },
        { 0x0000003e, Bo_Gt },
        { 0x00002626, Bo_And },
        { 0x0000005e, Bo_BXor },
        { 0x0000002a, Bo_Mul },
        { 0x0000002f, Bo_Div },
        { 0x00003e3e, Bo_Shr },
        { 0x00003d3c, Bo_Le },
        { 0x0000002b, Bo_Plus },
        { 0x00007c7c, Bo_Or },
        { 0x00003d21, Bo_Ne },
    };
    
#define NUM_BO_DISPS 4
    static const uint32_t _bo_disps[NUM_BO_DISPS][2] = 
        { { 1, 6 }, { 10, 11 }, { 2, 0 }, { 1, 2 },  };
    static const char* _bo_hashkey = "\x00\x00\x00\x00\x00\x00\x00\x00\x01\x9a\x8aq\xc9.$8";

    uint64_t hash = thirdparty_siphash24(&in, sizeof(in), _bo_hashkey);
    const uint32_t lower = hash & 0xffffffff;
    const uint32_t upper = (hash >> 32) & 0xffffffff;

    const uint32_t g = (lower >> 16);
    const uint32_t f1 = lower;
    const uint32_t f2 = upper;

    const uint32_t *d = _bo_disps[(g % NUM_BO_DISPS)];
    const uint32_t idx = (d[1] + f1 * d[0] + f2) % NUM_ENTRIES_BINARY_OP;
    const struct _bo_struct_tuple entry = _bo_entries[idx];

    if (entry._0 != in) {
        return Bo_Invalid;
    }
    return entry._1;

#undef NUM_BO_DISPS
}
#endif //OPERATORS_H_IMPLEMENTATION

#define NUM_ENTRIES_ASSIGNMENT_OP 14

typedef enum {
    Ao_Invalid = -1,
    Ao_Assign,
    Ao_WalrusAssign,
    Ao_PlusAssign,
    Ao_MinusAssing,
    Ao_MulAssign,
    Ao_DivAssign,
    Ao_ModAssign,
    Ao_BOrAssign,
    Ao_BAndAssign,
    Ao_BXorAssign,
    Ao_ShlAssign,
    Ao_ShrAssign,
    Ao_AndAssign,
    Ao_OrAssign,
    Ao_NumberOfElements
} AssignmentOp;

static_assert(Ao_NumberOfElements == NUM_ENTRIES_ASSIGNMENT_OP, "Number of enum AssignmentOp elements changed. This file was generated by operators.h.templ8, edit this instead");

const char *assignment_op_to_string(AssignmentOp in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
const char *assignment_op_to_string(AssignmentOp in) {
    static const char *assignment_op_str_map[NUM_ENTRIES_ASSIGNMENT_OP] = {
        "Assign",
        "WalrusAssign",
        "PlusAssign",
        "MinusAssing",
        "MulAssign",
        "DivAssign",
        "ModAssign",
        "BOrAssign",
        "BAndAssign",
        "BXorAssign",
        "ShlAssign",
        "ShrAssign",
        "AndAssign",
        "OrAssign",
    };
    assert(in < NUM_ENTRIES_ASSIGNMENT_OP);

    return assignment_op_str_map[in];
}
#endif //OPERATORS_H_IMPLEMENTATION

AssignmentOp assignment_op_resolve(uint32_t in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
AssignmentOp assignment_op_resolve(uint32_t in) {
    static const struct _ao_struct_tuple { uint32_t _0; AssignmentOp _1; } _ao_entries[NUM_ENTRIES_ASSIGNMENT_OP] = {
        { 0x003d3e3e, Ao_ShrAssign },
        { 0x00003d2b, Ao_PlusAssign },
        { 0x00003d2f, Ao_DivAssign },
        { 0x00003d26, Ao_AndAssign },
        { 0x003d3c3c, Ao_ShlAssign },
        { 0x003d2626, Ao_BAndAssign },
        { 0x003d7c7c, Ao_BOrAssign },
        { 0x00003d5e, Ao_BXorAssign },
        { 0x00003d3a, Ao_WalrusAssign },
        { 0x00003d25, Ao_ModAssign },
        { 0x0000003d, Ao_Assign },
        { 0x00003d2d, Ao_MinusAssing },
        { 0x00003d7c, Ao_OrAssign },
        { 0x00003d2a, Ao_MulAssign },
    };
    
#define NUM_AO_DISPS 3
    static const uint32_t _ao_disps[NUM_AO_DISPS][2] = 
        { { 1, 1 }, { 3, 0 }, { 8, 9 },  };
    static const char* _ao_hashkey = "\x00\x00\x00\x00\x00\x00\x00\x00P\xdb\xb1\xeb\x9a\xa1K\x95";

    uint64_t hash = thirdparty_siphash24(&in, sizeof(in), _ao_hashkey);
    const uint32_t lower = hash & 0xffffffff;
    const uint32_t upper = (hash >> 32) & 0xffffffff;

    const uint32_t g = (lower >> 16);
    const uint32_t f1 = lower;
    const uint32_t f2 = upper;

    const uint32_t *d = _ao_disps[(g % NUM_AO_DISPS)];
    const uint32_t idx = (d[1] + f1 * d[0] + f2) % NUM_ENTRIES_ASSIGNMENT_OP;
    const struct _ao_struct_tuple entry = _ao_entries[idx];

    if (entry._0 != in) {
        return Ao_Invalid;
    }
    return entry._1;

#undef NUM_AO_DISPS
}
#endif //OPERATORS_H_IMPLEMENTATION

#define NUM_ENTRIES_UNARY_OP 6

typedef enum {
    Uo_Invalid = -1,
    Uo_BitwiseNot,
    Uo_Not,
    Uo_Plus,
    Uo_Minus,
    Uo_Address,
    Uo_Deref,
    Uo_NumberOfElements
} UnaryOp;

static_assert(Uo_NumberOfElements == NUM_ENTRIES_UNARY_OP, "Number of enum UnaryOp elements changed. This file was generated by operators.h.templ8, edit this instead");

const char *unary_op_to_string(UnaryOp in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
const char *unary_op_to_string(UnaryOp in) {
    static const char *unary_op_str_map[NUM_ENTRIES_UNARY_OP] = {
        "BitwiseNot",
        "Not",
        "Plus",
        "Minus",
        "Address",
        "Deref",
    };
    assert(in < NUM_ENTRIES_UNARY_OP);

    return unary_op_str_map[in];
}
#endif //OPERATORS_H_IMPLEMENTATION

UnaryOp unary_op_resolve(uint32_t in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
UnaryOp unary_op_resolve(uint32_t in) {
    static const struct _uo_struct_tuple { uint32_t _0; UnaryOp _1; } _uo_entries[NUM_ENTRIES_UNARY_OP] = {
        { 0x0000002b, Uo_Plus },
        { 0x00000026, Uo_Address },
        { 0x0000007e, Uo_BitwiseNot },
        { 0x0000002a, Uo_Deref },
        { 0x0000002d, Uo_Minus },
        { 0x00000021, Uo_Not },
    };
    
#define NUM_UO_DISPS 2
    static const uint32_t _uo_disps[NUM_UO_DISPS][2] = 
        { { 2, 0 }, { 0, 5 },  };
    static const char* _uo_hashkey = "\x00\x00\x00\x00\x00\x00\x00\x00P\xdb\xb1\xeb\x9a\xa1K\x95";

    uint64_t hash = thirdparty_siphash24(&in, sizeof(in), _uo_hashkey);
    const uint32_t lower = hash & 0xffffffff;
    const uint32_t upper = (hash >> 32) & 0xffffffff;

    const uint32_t g = (lower >> 16);
    const uint32_t f1 = lower;
    const uint32_t f2 = upper;

    const uint32_t *d = _uo_disps[(g % NUM_UO_DISPS)];
    const uint32_t idx = (d[1] + f1 * d[0] + f2) % NUM_ENTRIES_UNARY_OP;
    const struct _uo_struct_tuple entry = _uo_entries[idx];

    if (entry._0 != in) {
        return Uo_Invalid;
    }
    return entry._1;

#undef NUM_UO_DISPS
}
#endif //OPERATORS_H_IMPLEMENTATION

bool check_is_punctuator(const char * in);

#ifdef   OPERATORS_H_IMPLEMENTATION
OPERATORS_H_PREFIX
bool check_is_punctuator(const char * in) {
    static const char * set_elements[48] = {
        "[", "/=", "/", ";", ">>", "->", "+=", "=", "%", "|=", "<<", "*", "-=", ",", "<=", ".", "&", "<", ">", "*=", "^=", "==", "&=", "!=", ":", "|", "||", ">=", "~", ">>=", ":=", "]", "?", ")", "::", "^", "+", "<<=", "%=", "}", "&&=", "..", "&&", "||=", "-", "{", "!", "(", 
    };

#define N_DISPS 10
    static const uint32_t displacements[N_DISPS][2] = 
        { { 0, 8 }, { 0, 41 }, { 0, 8 }, { 0, 15 }, { 1, 5 }, { 4, 7 }, { 0, 36 }, { 1, 8 }, { 33, 14 }, { 0, 0 },  };
    static const char* _check_is_punctuator_hashkey = "\x00\x00\x00\x00\x00\x00\x00\x00\xb9\x9d\xe1\xceY\x19lN";

    uint64_t hash = thirdparty_siphash24(in, strlen(in), _check_is_punctuator_hashkey);
    const uint32_t lower = hash & 0xffffffff;
    const uint32_t upper = (hash >> 32) & 0xffffffff;

    const uint32_t g = (lower >> 16);
    const uint32_t f1 = lower;
    const uint32_t f2 = upper;

    const uint32_t *d = displacements[(g % N_DISPS)];
    const uint32_t idx = (d[1] + f1 * d[0] + f2) % 48;
    const char * entry = set_elements[idx];

    return strcmp(entry, in) == 0;
#undef N_DISPS
}
#endif //OPERATORS_H_IMPLEMENTATION

#define L_BRACE '{'
#define R_BRACE '}'
#define L_BRACKET '['
#define R_BRACKET ']'
#define L_PAREN '('
#define R_PAREN ')'
#define COLON ':'
#define COMMA ','
#define DOT '.'
#define SEMICOLON ';'
#define QUESTION '?'
#define DOUBLE_COLON 0x00003a3a
#define DOUBLE_DOT 0x00002e2e
#define DOUBLE_AND 0x00002626
#define ARROW 0x00003e2d

#endif //OPERATORS_H_