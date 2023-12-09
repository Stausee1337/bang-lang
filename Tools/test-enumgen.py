import re
import sys
from dataclasses import dataclass
from typing import List, Tuple

from genhash import HashState, format_bytes_as_c_string, generate_whatever, ikey_to_bkey

@dataclass
class VariantDef:
    name: str
    args: Tuple[bytes, int]

@dataclass
class EnumDef:
    name: str
    defs: List[VariantDef]


def expect(tok: Token, e_kind: TokenKind, value: str|None = None):
    if tok.kind != e_kind:
        print(f"In line {tok.lineno}: expected {e_kind!r}, found {tok.kind!r}")
        exit(1)
    if value is None:
        return
    if tok.value != value:
        print(f"In line {tok.lineno}: expected {value}, found {tok.value}")
        exit(1)

def tohex(data: bytes) -> str:
    n = int.from_bytes(data, byteorder='little')
    return f'0x{n:08x}'

def gen_mapping(enum: EnumDef):
    prefix = ''.join(c for c in enum.name if c.isupper()).lower()
    tprefix = prefix.title()
    print(f'{enum.name} _{prefix}_item_hash(int token) {{') #}}
    print('    switch (token) {')
    for vdef in enum.defs:
        print(f'        case {tohex(vdef.args[0])}: return {tprefix}_{vdef.name};')
    print(f'        default: return {tprefix}_Invalid;')
    print('    }')
    print('}')

def gen_c_resolve_fn(enum: EnumDef, hstate: HashState):
    lower_name = '_'.join(p.lower() for p in re.findall('[A-Z][a-z]*', enum.name))
    lower_prefix = ''.join(c for c in enum.name if c.isupper()).lower()
    title_prefix = lower_prefix.title()
    upper_prefix = title_prefix.upper() 
    constant_name = f'_{upper_prefix}_N_ELEMENTS'

    print(f'static {enum.name} {lower_name}_from_toktyp(uint32_t toktyp) {{') #}}
    entries_name = f'_{lower_prefix}_entries'
    struct_name = f'_{lower_prefix}_struct_tupl3'
    print(f'    static const struct {struct_name} {{ uint32_t _0; {enum.name} _1; }} {entries_name}[{constant_name}] = {{') #}}
    for idx in hstate.idx_map:
        vdef = enum.defs[idx]
        print(f'        {{ {tohex(vdef.args[0])}, {title_prefix}_{vdef.name} }},')
    print('    };')

    disps_constant_name = f'_{upper_prefix}_N_DISPS'
    print(f'#define {disps_constant_name} {len(hstate.disps)}')
    disps_name = f'_{lower_prefix}_disps'
    disps_array_format = ', '.join('{%d, %d}' % disp for disp in hstate.disps)
    print(f'    static const uint32_t {disps_name}[{disps_constant_name}][2] = {{ {disps_array_format} }};')
    hashkey_name = f'_{lower_prefix}_hashkey'
    bkey = ikey_to_bkey(hstate.key)
    print(f'    static const char* {hashkey_name} = {format_bytes_as_c_string(bkey)};')
    print();
    print(f'    uint64_t hash = thirdparty_siphash24(&toktyp, sizeof(toktyp), {hashkey_name});')
    print('''\
    const uint32_t lower = hash & 0xffffffff;
    const uint32_t upper = (hash >> 32) & 0xffffffff;

    const uint32_t g = (lower >> 16);
    const uint32_t f1 = lower;
    const uint32_t f2 = upper;
''')
    
    print(f'    const uint32_t *d = {disps_name}[(g % {disps_constant_name})];') 
    print(f'    const uint32_t idx = (d[1] + f1 * d[0] + f2) % {constant_name};')
    print()

    print(f'    const struct {struct_name} entry = {entries_name}[idx];')
    print(f'''\
    if (entry._0 != toktyp) {{
        return {title_prefix}_Invalid;
    }}
    return entry._1;
''')

    print(f'#undef {disps_constant_name}')
    print('}')

def gen_c_code(enum: EnumDef):
    prefix = ''.join(c for c in enum.name if c.isupper()).lower().title()
    constant_name = f'_{prefix.upper()}_N_ELEMENTS'
    print(f'#define {constant_name} {len(enum.defs)}')
    print()
    print('typedef enum {') # }
    print(f'    {prefix}_Invalid = -1,') 
    for vdef in enum.defs:
        print(f'    {prefix}_{vdef.name},') 
    print(f'    {prefix}_NumberOfElements') 
    print(f'}} {enum.name};') 
    print()
    print(f'static_assert({prefix}_NumberOfElements == {constant_name}, "Number of {enum.name} elements changed; dont change the header change the template");')
    print()
    print(f'static unsigned char _{prefix.lower()}_operator_precedence[{constant_name}] = {{')
    print(f'    {", ".join(str(x.args[1]) for x in enum.defs)} }};')
    print()

    gen_c_resolve_fn(enum, generate_whatever([
        int.from_bytes(vdef.args[0], byteorder='little') for vdef in enum.defs]))

    # gen_mapping(enum)
    # print()
    # print(f'static int _{prefix.lower()}_token_type[{constant_name}] = {{')
    # print(f'    {", ".join(tohex(x.args[0]) for x in enum.defs)} }};')
    # print()

def gen_rust_code(enum: EnumDef):
    print(f'enum {enum.name} {{') # }}
    for vdef in enum.defs:
        print(f'    {vdef.name},') 
    print('}') 
    print()

    upper_name = '_'.join(p.upper() for p in re.findall('[A-Z][a-z]*', enum.name))

    print(f'const {upper_name}_MAP: phf::Map<u32, {enum.name}> = phf_map! {{') #}}
    for vdef in enum.defs:
        print(f'    {tohex(vdef.args[0])}u32 => {enum.name}::{vdef.name},')
    print('};')
    # print()
    # print(f'static int _{prefix.lower()}_token_type[{len(enum.defs)}] = {{')
    # print(f'    {", ".join(tohex(x.args[0]) for x in enum.defs)} }};')
    print()

def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <file>')
        exit(1)

    with open(sys.argv[1], 'r') as f_in:
        contents = f_in.read()
    stream = iter(tokenize(contents))

    while True:
        try:
            tok = next(stream)
        except StopIteration:
            break
        expect(tok, TokenKind.Identifier, "enum")
        tok = next(stream)
        expect(tok, TokenKind.Identifier)
        enum_name = tok.value

        tok = next(stream)
        expect(tok, TokenKind.LBrace)

        variant_defs: List[VariantDef] = []
        while True:
            tok = next(stream)
            if tok.kind == TokenKind.RBrace:
                break
            expect(tok, TokenKind.Identifier)
            field_name = tok.value

            tok = next(stream)
            expect(tok, TokenKind.LParen)
            binstr = b''

            while True:
                tok = next(stream)
                if tok.kind == TokenKind.Comma:
                    break
                expect(tok, TokenKind.Tok)
                binstr += tok.value.encode()
            tok = next(stream)
            expect(tok, TokenKind.Number)
            number = int(tok.value)

            tok = next(stream)
            expect(tok, TokenKind.RParen)

            tok = next(stream)
            expect(tok, TokenKind.Semi)

            variant_defs.append(VariantDef(name=field_name, args=(binstr, number)))

        gen_c_code(EnumDef(name=enum_name, defs=variant_defs))




if __name__ == '__main__':
    main()

