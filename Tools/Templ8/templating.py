import builtins
from dataclasses import dataclass
from importlib import import_module
import io
import os
import sys
import inspect
import linecache
from types import CodeType, FrameType, FunctionType, MethodType, ModuleType, TracebackType
from typing import Any, Callable, Generator, Iterable, Literal, Type, TypeVar
from importlib.machinery import PathFinder, SourceFileLoader
from typing_extensions import Self

from Templ8.compiler import CodeGenerator
import Templ8.nodes as n
from Templ8.lexing import TOK_INITIAL, LimitedException, Pos, expect, test, tokenize, tokenize_line, register_tokens, tokenwraps, TokenStream, Token, find_next, unexpected_error, WSyntaxError
from Templ8.wexpressions import ensure_arguments, parse_tuple, parse_assign_lhs

from rich import print as rprint
from rich.syntax import Syntax

TOK_BLOCK_BEGIN = "block_begin"
TOK_BLOCK_END = "block_end"
TOK_VARIABLE_BEGIN = "variable_begin"
TOK_VARIABLE_END = "variable_end"
TOK_TEXT = "text"

register_tokens(TOK_BLOCK_BEGIN, TOK_BLOCK_END, TOK_VARIABLE_BEGIN, TOK_VARIABLE_END, TOK_TEXT)

def method(f):
    f.__ismethod__ = True
    return f

def is_visible(obj, /) -> bool:
    if isinstance(obj, MethodType):
        obj = obj.__func__
    if getattr(obj, '__ismethod__', False):
        return False
    return True

class Macro:
    __slots__ = ('__func__', )

    def __init__(self, function: FunctionType, /):
        self.__func__ = function

    def __repr__(self) -> str:
        return f'<macro wrapper for {self.__func__.__name__}>'

    def __call__(self, *args, **kwargs):
        return self.__func__(*args, **kwargs)

class Context(dict[str, Any]):
    def __init__(self, input_filename: str, output_filename: str):
        self.out = io.StringIO()
        self.top_lines = []
        self.bottom_lines = []
        self.input_filename = input_filename
        self.output_filename = output_filename
        self.macros: dict[str, Macro] = {}
        self.exports: dict[str, Any] = {}
        self.update(builtins.__dict__)
        self.__FILENAME__ = os.path.basename(self.input_filename)

    @classmethod
    def from_base_context(cls, input_filename: str, base: Self) -> Self:
        res = cls(input_filename, base.output_filename)
        res.out = base.out
        res.top_lines = base.top_lines
        res.bottom_lines = base.bottom_lines
        res.__FILENAME__ = base.__FILENAME__
        return res
    
    def qappend(self, l: list, item):
        assert isinstance(l, list), "First Argument must be list"
        if item not in l:
            l.append(item)

    def emit(self, content):
        self.out.write(str(content))

    def macrodef(self, macrofn: FunctionType):
        self.macros[macrofn.__name__] = Macro(macrofn)

    def snake_case(self, string: str, /) -> str:
        import re
        return '_'.join(p.lower() for p in re.findall('[A-Z][a-z]+', string))

    def prefix(self, string: str, /) -> str:
        return ''.join(c for c in string if c.isupper())

    def lower(self, string: str, /) -> str:
        assert isinstance(string, str), 'lower expected string'
        return string.lower()

    def upper(self, string: str, /) -> str:
        assert isinstance(string, str), 'upper expected string'
        return string.upper()

    def ilen(self, obj) -> str:
        return f'NUM_ENTRIES_{self.snake_case(obj.name).upper()}'

    def title(self, string: str, /) -> str:
        return string.title()

    def invaliddef(self, obj):
        return f'{self.prefix(obj.name).title()}_Invalid'

    def to_string(self) -> str: 
        return '\n'.join(self.top_lines) + '\n' + self.out.getvalue() + '\n' + '\n'.join(self.bottom_lines)

    def resolve_macro(self, name: str) -> Macro:
        if (macro := self.macros.get(name)) is not None:
            return macro
        raise NameError(f'name {name!r} is not defined')

    def import_names(self, names: dict[str, Any]):
        for name, value in names.items():
            if isinstance(value, Macro):
                self.macros[name] = value
            else: 
                self[name] = value

    def export(self, **kwargs: Any):
        self.exports.update(kwargs)

    def __getitem__(self, key: str) -> Any:
        if (res := dict.get(self, key)) is not None:
            return res
        if (res := getattr(self, key, None)) is not None and is_visible(res):
            return res
        raise KeyError(key)

def test_fn(buf: str, *, parse_assign=False) -> n.Expression:
    stream = tokenize_line(f'buf-{hash(buf)}', buf, Pos(0, 0))
    next(stream)
    return parse_tuple(stream) if not parse_assign else parse_assign_lhs(stream)

def get_buffered_value(buffered: Token|None, /) -> str:
    return buffered.value if buffered is not None else ''

@tokenwraps
def tokenize_buf(fname: str, buf: str) -> Generator[Token, None, None]:
    lines = buf.splitlines(keepends=True)
    yield Token(TOK_INITIAL, buf, (start := Pos(len(lines) - 1, 0)), start, fname)
    buffered: Token|None = None
    remove_buffered_newline: Literal['no', 'block_line_not_emitted', 'also_found_newline_line'] = 'no'
    for lineno, line in enumerate(lines):
        col = 0
        tokenized_line: Iterable[Token] = []
        while True:
            if (tok := find_next(line, col, '{{', '{%')) is None: #}}}
                break 
            start_typ, start_col = tok

            if (pre_txt := line[col:start_col]) != '':
                tokenized_line.append(Token(
                    TOK_TEXT, pre_txt, 
                    Pos(lineno, col), 
                    Pos(lineno, start_col), fname))

            end_typ = '}}' if start_typ == '{{' else '%}' # }
            if (res := find_next(line, start_col + 2, end_typ)) == None:
                raise WSyntaxError((Pos(lineno, col), fname), 'unclosed block')
            col = res[1]
            
            start_kind = TOK_VARIABLE_BEGIN if start_typ == '{{' else TOK_BLOCK_BEGIN #}}
            tokenized_line.append(Token(start_kind, start_typ, (start := Pos(lineno, start_col)), start + 2, fname))

            start_col += 2
            substream = tokenize_line(fname, line[start_col:col], Pos(lineno, start_col))
            substream.emit_eof = False
            tokenized_line.extend(substream)
            end_kind = TOK_VARIABLE_END if end_typ == '}}' else TOK_BLOCK_END
            tokenized_line.append(Token(end_kind, end_typ, (start := Pos(lineno, col)), start + 2, fname))

            col += 2
            # yield Token(TOK_TEXT, txt, start+2, start+2+len(txt), fname)
        post_txt = line[col:]
        line_contains_variable = any(token.kind == TOK_VARIABLE_BEGIN for token in tokenized_line)
        line_contains_block = any(token.kind == TOK_BLOCK_BEGIN for token in tokenized_line)
        if not line_contains_variable and line_contains_block:
            line_text = (
                ''.join(token.value for token in tokenized_line if token.kind == TOK_TEXT) +
                post_txt)
            line_emitting = line_text != '' and not line_text.isspace()
        else:
            line_emitting = True 


        # print(repr(line), line_emitting, repr(get_buffered_value(buffered)))
        if not line_emitting:
            # the line is empty or only spaces; remove all text tokens, they won't be in the output
            tokenized_line = list(token for token in tokenized_line if token.kind != TOK_TEXT)

        if not line_emitting:
            if remove_buffered_newline == 'no':
                remove_buffered_newline = 'block_line_not_emitted'
            elif remove_buffered_newline == 'also_found_newline_line':
                assert get_buffered_value(buffered).endswith('\n')
                # print(repr(get_buffered_value(buffered)), repr(line))
                buffered.value = buffered.value[:-1]
                remove_buffered_newline = 'no'
        else:
            if len(tokenized_line) == 0 and post_txt == '\n' and remove_buffered_newline == 'block_line_not_emitted':
                # print(repr(line))
                remove_buffered_newline = 'also_found_newline_line'
            else:
                remove_buffered_newline = 'no'

        if len(tokenized_line) > 0 and buffered is not None:
            # print(f'inbetween flush {buffered.value!r}', tokenized_line)
            first_token = tokenized_line[0]
            if first_token.kind == TOK_TEXT:
                first_token.start = buffered.start
                first_token.value = buffered.value + first_token.value
            else:
                tokenized_line.insert(
                    0, Token(TOK_TEXT, buffered.value, buffered.start, buffered.end, fname)
                )
            # yield (tok )
            buffered = None
        #print(repr(line), line_emitting, tokenized_line[0] if len(tokenized_line) > 0 else None)
        yield from tokenized_line

        if post_txt != '' and line_emitting:
            if buffered is None:
                buffered = Token(TOK_TEXT, post_txt, (start := Pos(lineno, col)), start + len(post_txt), fname)
            else:
                buffered.value += post_txt
                buffered.end = Pos(lineno, col + len(post_txt))
    if buffered is not None and buffered.value != '':
        # print(f'final flush {buffered.value!r}')
        yield Token(TOK_TEXT, buffered.value, (start := Pos(lineno, col)), start, fname)

def check_block(stream: TokenStream, *args: str) -> bool:
    if stream.current.kind != TOK_BLOCK_BEGIN:
        return False
    return test(stream.lookahead(1), f'ident:{" ".join(args)}')

TExpr = TypeVar('TExpr')
def parse_templating_block(
        stream: TokenStream,
        context: Context,
        kind: str, expression_parser: Callable[[str, TokenStream], TExpr|None],
        *, stoppers: list[str]
    ) -> tuple[TExpr, n.Block]|n.Block:
    assert len(stoppers) > 0
    starttok = stream.current
    assert test(stream.current, f'ident:{kind}'), f"`{kind}` token present for {kind} block"
    next(stream)
    expr = expression_parser(kind, stream.fork_limited(eof_detect='block_end:'))
    assert test((endblock := stream.current), 'block_end:'), f"Expected block end; Current {stream.current}"
    next(stream)

    try:
        block_expr_generator = templating(
                stream.fork_limited(eof_detect=lambda s: check_block(s, *stoppers)), context)
        statement_expressions: list[n.Node] = list(block_expr_generator)
    except LimitedException:
        if len(stoppers) == 1:
            ending_message = repr(stoppers[0])
        else:
            ending_message = f'any of {", ".join(repr(stopper) for stopper in stoppers[:-1])} or {stoppers[-1]}'
        raise WSyntaxError(
            (endblock.end, stream.filename),
            f'´{starttok.value}´ block is never closed with {ending_message}'
        )
    assert test(stream.current, 'block_begin:'), f"Fork limmited failed; Current: {stream.current}"
    ll = stream.lookahead(1)
    assert test(ll, f'ident:{" ".join(stoppers)}'), f"WTF??? My brain is not braining"
    assert ll is not None, "unreachable"

    start = starttok.start
    end = ll.end
    if expr is not None:
        return expr, n.Block(statement_expressions, start=start, end=end)
    return n.Block(statement_expressions, start=start, end=end)

def parse_text_block(
        stream: TokenStream,
        kind: str, expression_parser: Callable[[str, TokenStream], TExpr|None],
        stopper: str
    ) -> tuple[TExpr, Token|None]|(Token|None):
    assert test(stream.current, f'ident:{kind}'), f"`{kind}` token present for {kind} block"
    next(stream)
    expr = expression_parser(kind, stream.fork_limited(eof_detect='block_end:'))
    assert test((endblock := stream.current), 'block_end:'), f"Expected block end; Current {stream.current}"

    starttok = next(stream)
    if not test(starttok, 'block_begin:'):
        while not stream.is_eof():
            next(stream)
            if check_block(stream, stopper):
                break
        else:
            raise WSyntaxError(
                (endblock.end, stream.filename),
                f'´{kind}´ block is never closed with {stopper!r}'
            )

        start = starttok.start
        end = stream.current.start
        text = stream.get_text_slice(start, end)
        texttok = Token(TOK_TEXT, text, start, end, stream.filename)
    else:
        texttok = Token(TOK_TEXT, '', starttok.start, starttok.start, stream.filename)

    assert test(stream.current, 'block_begin:'), f"Expected block begin"
    next(stream)
    assert test(stream.current, f'ident:{stopper}'), f"WTF??? My brain is not braining"
    next(stream)
    if not test(stream.current, f'block_end:'):
        unexpected_error(stream.current, 'block end')

    next(stream) # flush the block_end

    if expr is None:
        return texttok
    return expr, texttok

def parse_if_block(stream: TokenStream, context: Context) -> n.If: 
    assert test(stream.current, 'block_begin:'), "parse_if_block needs to be called with block_begin as stream.current"
    next(stream)

    starttok = stream.current

    kind = 'if'
    branches = []
    else_branch: n.Block|None = None
    while True:
        result = parse_templating_block(
            stream, context, kind,
            lambda k,s: None if k == 'else' else parse_tuple(s),
            stoppers=['elif', 'else', 'endif'])
        keyword = next(stream)
        assert test(keyword, 'ident:elif else endif'), "WTF??? My brain is not braining 2"
        if kind == 'else':
            assert not isinstance(result, tuple)
            else_branch = result
            if not test(keyword, 'ident:endif'):
                raise WSyntaxError(keyword, 'after `else` branch only `endif` keyword is allowed')
        else: 
            assert isinstance(result, tuple)
            branches.append(result)
            if test(keyword, 'ident:if'):
                assert False, "This should basically not be happing to us; It should nest"

        kind = keyword.value
        if kind == 'endif':
            endtok = keyword
            expect(stream, 'block_end:')
            next(stream)
            break

    return n.If(
        branches,
        else_branch,
        start=starttok.start, end=endtok.end)

def parse_for_block(stream: TokenStream, context: Context) -> n.For:
    class ForLoopInit(n.Node):
        bound_vars: n.Expression
        iterator_expr: n.Expression 

    def parse_for_assign(_: str, stream: TokenStream) -> n.Node:
        starttok = stream.current
        assign = parse_assign_lhs(stream.fork_limited(eof_detect='punct::'))
        if not test(stream.current, 'punct::'):
            unexpected_error(stream.current, '`:` associator', location=assign.end) 
        next(stream)
        expr = parse_tuple(stream)
        return ForLoopInit(assign, expr, start=starttok.start, end=stream.current.end)

    assert test(stream.current, 'block_begin:'), "parse_for_block needs to be called with block_begin as stream.current" 
    starttok = next(stream)

    result = parse_templating_block(
        stream, context, 'for', parse_for_assign,
        stoppers=['endfor'])

    expect(stream, 'ident:endfor')
    expect(stream, 'block_end:')
    next(stream)

    assert isinstance(result, tuple), "Internal things broken"
    init, body = result
    assert isinstance(init, ForLoopInit), "Internal things broken"

    return n.For(init.bound_vars, init.iterator_expr, body, start=starttok.start, end=stream.current.end)

def parse_macro_block(stream: TokenStream, context: Context) -> n.Macro:
    class MacroSignature(n.Node):
        name: str
        ins: list[str]

    def parse_macro_signature(_: str, stream: TokenStream) -> MacroSignature:
        if not test(stream.current, 'ident:'):
            unexpected_error(stream.current, 'macro identifier', location=starttok.end)
            assert False, "Unreachable"
        istarttok = stream.current
        name = stream.current.value
        if not name.isidentifier():
            raise WSyntaxError((starttok.end, stream.filename), 'macro name must be valid python identifier')
        endtok = next(stream)
        if not test(endtok, 'ident:in'):
            if not test(endtok, 'eof:'):
                unexpected_error(endtok, '`in` keyword or block end')
            return MacroSignature(name, [], start=istarttok.start, end=endtok.end) 
        inputs = []
        while True:
            endtok = stream.current
            if test(token := next(stream), 'eof:'):
                break
            if not test(token, 'ident:'):
                unexpected_error(token, 'input identifier')
            inputs.append(token.value)
        return MacroSignature(name, inputs, start=istarttok.start, end=endtok.end)

    assert test(stream.current, 'block_begin:'), "parse_macro_block needs to be called with block_begin as stream.current"
    starttok = next(stream)

    result = parse_templating_block(
        stream, context, 'macro', parse_macro_signature,
        stoppers=['endmacro'])

    expect(stream, 'ident:endmacro')
    expect(stream, 'block_end:')
    next(stream)

    assert isinstance(result, tuple), "Internal things broken"
    sig, body = result
    assert isinstance(sig, MacroSignature), "Internal things broken"

    return n.Macro(sig.name, sig.ins, body, start=starttok.start, end=stream.current.end)

def parse_expand(stream: TokenStream) -> n.Expression:
    assert test(stream.current, 'block_begin:'), "parse_expand needs to be called with block_begin as stream.current"
    next(stream)
    assert test(stream.current, 'ident:expand'), "ident needs to be exand"
    macro_identifier = next(stream)
    if not macro_identifier.value.isidentifier():
        raise WSyntaxError(macro_identifier, 'expand identifier needs to be valid python identifier')
    starttok =  next(stream) 

    arguments: list[n.Expression] = []
    while True:
        if test(stream.current, 'block_end:'):
            break
        arguments.append(parse_tuple(stream))
    next(stream)

    if len(arguments) > 0:
        argstart = arguments[0].start
        argend = arguments[-1].end
    else:
        argstart = argend = macro_identifier.end + 1

    invalispan = dict(
        start=(invalipos := Pos(starttok.start.lineno, -1)),
        end=invalipos
    )
    resolve_macro = n.Variable('resolve_macro', **invalispan)

    return n.Call(
        n.Call(resolve_macro, ensure_arguments(n.Literal(macro_identifier.value, **macro_identifier)), **invalispan),
        n.CallArguments(arguments, {}, start=argstart, end=argend),
        start=starttok.start, end=stream.current.end)

def parse_def(stream: TokenStream) -> n.Node:
    assert test(stream.current, 'block_begin:'), "parse_def needs to be called with block_begin as stream.current"
    starttok = next(stream)
    assert test(stream.current, 'ident:def'), "ident needs to be def"
    next(stream)
    store_global = False
    if test(stream.current, 'punct:@'):
        store_global = True
        global_tok = expect(stream, 'ident:global')
        next(stream)

    lhs = parse_assign_lhs(stream.fork_limited(eof_detect='punct:='))
    if not test(stream.current, 'punct:='):
        unexpected_error(stream.current, '=', location=lhs.end)
    next(stream)
    if store_global and not isinstance(lhs, n.Variable):
        raise WSyntaxError(global_tok, f'Cannot store {lhs.printable_name} as a global')
    rhs = parse_tuple(stream.fork_limited(eof_detect='block_end:'))

    if not test(stream.current, 'block_end:'):
        unexpected_error(stream.current, 'block_end')
    next(stream)

    assign = n.Assign(lhs, rhs, start=starttok.start, end=stream.current.end)
    if store_global:
        return n.Block([n.Global([lhs.name], **global_tok), assign], start=global_tok.start, end=stream.current.end)
    return assign

def parse_eval(stream: TokenStream) -> n.Expression:
    assert test(stream.current, 'block_begin:'), "parse_eval needs to be called with block_begin as stream.current"
    next(stream)
    assert test(stream.current, 'ident:eval'), "ident needs to be eval"
    next(stream)

    expression = parse_tuple(stream.fork_limited(eof_detect='block_end:'))

    if not test(stream.current, 'block_end:'):
        unexpected_error(stream.current, 'block_end')
    next(stream)

    return expression

def update_firstlineno(code: CodeType, lineno: int) -> CodeType:
    consts = tuple(
        update_firstlineno(const, lineno) if isinstance(const, CodeType) else const for const in code.co_consts)
    return code.replace(
        co_firstlineno=code.co_firstlineno + lineno,
        co_consts=consts
    )

def define_module(stream: TokenStream, context: Context):
    assert test(stream.current, 'block_begin:'), "define_module needs to be called with block_begin as stream.current"
    next(stream)

    def expect_identifier(_: str, stream: TokenStream) -> str:
        if not test((nametok := stream.current), 'ident:'):
            unexpected_error(stream.current, 'module name', location=stream.current.end)
        next(stream)
        return nametok.value

    result = parse_text_block(stream, 'pymodule', expect_identifier, stopper='endmodule')
    assert isinstance(result, tuple)
    name, text = result 

    from importlib._bootstrap import _init_module_attrs
    from importlib.machinery import ModuleSpec

    module = ModuleType(name)
    spec = ModuleSpec(name, None, origin=stream.filename)
    spec.has_location = True
    _init_module_attrs(spec, module)

    if text is not None:
        try:
            code = compile(text.value, stream.filename, 'exec')
        except SyntaxError as err:
            assert err.lineno is not None
            err.lineno += text.start.lineno
            raise err
        code = update_firstlineno(code, text.start.lineno)
        exec(code, module.__dict__)

    sys.modules[module.__name__] = module
    context[module.__name__] = module

def check_template_valid_library(location: Token|tuple[Pos, str], filename: str) -> str:
    template, ext = os.path.splitext(filename)
    if ext != '.templ8':
        raise WSyntaxError(location, 'template library file must end with `.templ8` extension')
    if '.' in template:
        raise WSyntaxError(location, 'template library file is not allowed to have `.` within its name')
    return template

def include_file(stream: TokenStream, context: Context):
    assert test(stream.current, 'block_begin:'), "include_file needs to be called with block_begin as stream.current"
    next(stream)
    assert test(stream.current, 'ident:include'), "ident needs to be include"
    next(stream)

    if not test((include_file_tok := stream.current), 'str:'):
        unexpected_error(stream.current, 'file path as string')
    next(stream)

    if not test(stream.current, 'block_end:'):
        unexpected_error(stream.current, 'block_end')
    next(stream)

    include_filename = include_file_tok.value[1:-1].encode().decode('unicode_escape')

    template = check_template_valid_library(include_file_tok, include_filename)
    if (template_ctx := sys.templates_cache.get(include_filename)) is None:
        spec = PathFinder.find_spec(template)

        if spec is None or not isinstance(spec.loader, TemplateFileLoader):
            raise WSyntaxError(include_file_tok, f'cannot find template library {include_filename!r}')
        
        assert spec.origin is not None
        template_ctx = process_single_file(
            spec.origin,
            spec.loader.get_data(spec.origin).decode(),
            None, base_context=context)

        sys.templates_cache[include_filename] = template_ctx

    context.import_names(template_ctx.exports)

def parse_customcode(stream: TokenStream, context: Context):
    assert test(stream.current, 'block_begin:'), "define_module needs to be called with block_begin as stream.current"
    next(stream)

    @dataclass
    class CustomCodeInfo:
        parser_expr: n.Expression
        result_variable: str|None

    def parse_head(_: str, stream: TokenStream) -> CustomCodeInfo:
        expr = parse_tuple(stream)
        result_variable = None
        if test(stream.current, 'ident:as'):
            result_variable = expect(stream, 'ident:').value
            next(stream)
        assert test(stream.current, 'eof:')
        return CustomCodeInfo(expr, result_variable)

    result = parse_text_block(stream, 'customcode', parse_head, stopper='endcode')
    assert isinstance(result, tuple)

    init, texttok = result
    if texttok is not None:
        gen = CodeGenerator(stream.filename)
        code = gen.compile(init.parser_expr, mode='eval')
        parser = eval(code, context)

        stream = tokenize(stream.filename, texttok.value, lineno=texttok.start.lineno, col=texttok.start.col)
        parsed_result = parser(stream)

        if init.result_variable is not None:
            context[init.result_variable] = parsed_result

def export_block(stream: TokenStream, context: Context) -> n.Call:
    assert test((startok := stream.current), 'block_begin:'), "export_block needs to be called with block_begin as stream.current"
    next(stream)
    assert test((exporttok := stream.current), 'ident:export'), "ident needs to be export"
    next(stream)

    check_template_valid_library(exporttok, context.input_filename)

    exports: list[tuple[str, bool]] = []
    while True:
        if test(token := stream.current, 'block_end:'):
            break

        is_macro = False
        if test(token, 'punct:$'):
            is_macro = True
            next(stream)
        if not test(stream.current, 'ident:'):
            unexpected_error(token, 'identifier')
        exports.append((stream.current.value, is_macro))
        next(stream)

    assert test((endtok := stream.current), 'block_end:'), "unreachable"
    next(stream)

    invalispan = dict(
        start=(invalipos := Pos(exporttok.start.lineno, -1)),
        end=invalipos
    )

    resolve_macro = n.Variable('resolve_macro', **invalispan)
    kwargs: dict[str, n.Expression] = {
        name:
        n.Variable(name, **invalispan)
        if not is_macro else
        n.Call(resolve_macro, ensure_arguments(n.Literal(name, **invalispan)), **invalispan)
        for name, is_macro in exports
    } 

    return n.Call(
        n.Variable('export', **invalispan),
        n.CallArguments([], kwargs, **invalispan),
        start=startok.start,
        end=endtok.end
    )

def parse_import(stream: TokenStream, context: Context):
    assert test(stream.current, 'block_begin:'), "parse_import needs to be called with block_begin as stream.current"
    starttok = next(stream)
    assert test(stream.current, 'ident:pyimport'), "ident needs to be pyimport"
    next(stream)

    dotted_path: list[str] = []
    while True:
        if not test(stream.current, 'ident:'):
            unexpected_error(stream.current, 'identifier')
        dotted_path.append(stream.current.value)

        if test(ll := stream.lookahead(1), 'block_end:') or test(ll, 'ident:as'):
            next(stream)
            break

        expect(stream, 'punct:.')
        next(stream)

    assert len(dotted_path) > 0
    if len(dotted_path) > 1 and not test(stream.current, 'ident:as'):
        unexpected_error(stream.current, '`as`-keyword')

    alias = None
    if test(stream.current, 'ident:as'):
        alias = expect(stream, 'ident:').value
        next(stream)

    if not test(stream.current, 'block_end:'):
        unexpected_error(stream.current, 'end of block')

    module_path = '.'.join(dotted_path)
    module = import_module(module_path)

    if alias is not None:
        context[alias] = module
    else:
        context[module_path] = module

    next(stream)

block_parsers: dict[str, Callable[[TokenStream], n.Node|None]|Callable[[TokenStream, Context], n.Node|None]] = {
        'if': parse_if_block,
        'for': parse_for_block,
        'macro': parse_macro_block,
        'expand': parse_expand,
        'def': parse_def,
        'eval': parse_eval,
        'pymodule': define_module,
        'include': include_file,
        'export': export_block,
        'customcode': parse_customcode,
        'pyimport': parse_import
}

def parse_block(stream: TokenStream, context: Context) -> n.Node|None:
    assert test(stream.current, 'block_begin:'), "parse_block needs to be called with block_begin as stream.current"
    keyword = stream.lookahead(1)
    if not test(keyword, 'ident:'):
        unexpected_error(keyword or stream.current, 'block identifier')
        assert False, "unreachable"
    assert keyword is not None, "unreachable"
    if (parser := block_parsers.get(keyword.value)) is None:
        raise WSyntaxError(keyword, f'keyword {keyword.value!r} is not known')
    if len(inspect.signature(parser).parameters) == 2:
        return parser(stream, context)
    return parser(stream)

def subparse(block_type: str, stream: TokenStream, context: Context) -> n.Node|None:
    if block_type == TOK_VARIABLE_BEGIN:
        next(stream)
        lstream = stream.fork_limited(eof_detect='variable_end:')
        expr = parse_tuple(lstream)
        next(stream)
        return make_emit(expr)
    elif block_type == TOK_BLOCK_BEGIN:
        return parse_block(stream, context)
    else:
        assert False, "Unreachable"

def make_emit(expr: n.Expression, /) -> n.Expression:
    return n.Call(
        n.Variable('emit', start=(invalidpos := Pos(expr.start.lineno, -1)), end=invalidpos),
        n.CallArguments([expr], {}, start=expr.start, end=expr.end),
        start=expr.start, end=expr.end)

def templating(stream: TokenStream, context: Context) -> Generator[n.Node, None, None]:
    while not stream.is_eof():
        token = stream.current
        if token.kind in (TOK_BLOCK_BEGIN, TOK_VARIABLE_BEGIN):
            if (node := subparse(token.kind, stream, context)) is not None:
                yield node
        elif test(token, 'text:'):
            yield make_emit(n.Literal(token.value, **token))
            next(stream)
        else:
            assert False, f'Lexer should only yield text tokens here, {token}'

def create_template(filename: str, content: str, context: Context) -> n.Template:
    stream = tokenize_buf(filename, content)
    next(stream)
    starttok = stream.current
    body = list(templating(stream, context))
    return n.Template(
        body,
        start=starttok.start,
        end=stream.current.end
    )

def getline(frame: FrameType) -> tuple[str, str]:
    file = inspect.getsourcefile(frame)
    if file:
        # Invalidate cache if needed.
        linecache.checkcache(file)
    else:
        file = inspect.getfile(frame)
        # Allow filenames in form of "<something>" to pass through.
        # `doctest` monkeypatches `linecache` module to enable
        # inspection, so let `linecache.getlines` to be called.
        if not (file.startswith('<') and file.endswith('>')):
            raise OSError('source code not available')
    return file, linecache.getline(file, frame.f_lineno)

SYNTAX_MAP = {
        '.py': 'python',
        '.templ8': 'django'
}

def display_runtime_error(typ: Type[BaseException]|None, exc: BaseException|None, traceback: TracebackType|None): 
    assert typ is not None
    assert exc is not None
    assert traceback is not None
    rprint('Traceback (most recent call last):')
    while traceback is not None:
        frame = traceback.tb_frame
        name = frame.f_code.co_name
        fname, fline = getline(frame)
        if os.path.basename(fname) == 'templating.py':
            traceback = traceback.tb_next
            continue
        rprint(f'  File "{fname}", line {frame.f_lineno}, in {name}', file=sys.stderr)
        fline = fline.lstrip().rstrip('\n')
        syntax = SYNTAX_MAP.get(os.path.splitext(fname)[1], 'text')
        print('    ', end='', file=sys.stderr)
        rprint(
            Syntax(fline, syntax, theme='native', code_width=len(fline), background_color='default')
        , file=sys.stderr)
        # print(inspect.getsource(frame))
        traceback = traceback.tb_next
    rprint(f'{typ.__name__}: {str(exc)}', file=sys.stderr)

def display_templating_syntax_error(exc: WSyntaxError):
    line = linecache.getline(exc.filename(), exc.position().lineno + 1)
    prev_line_len = len(line)
    line = line.lstrip()
    number_of_leading_spaces = prev_line_len - len(line)
    line = line.rstrip('\n')
    rprint(f'  File "{exc.filename()}", at {exc.position().here()}', file=sys.stderr)
    print(f'    {line}', file=sys.stderr)
    print(f'    {" " * (exc.position().col - number_of_leading_spaces)}^', file=sys.stderr)
    rprint(f'Error: {exc}', file=sys.stderr)

def display_python_syntax_error(exc: SyntaxError):
    assert exc.lineno is not None
    assert exc.filename is not None
    line = linecache.getline(exc.filename, exc.lineno)
    prev_line_len = len(line)
    line = line.lstrip()
    number_of_leading_spaces = prev_line_len - len(line) - 1
    line = line.rstrip('\n')
    rprint(f'  File "{exc.filename}", at line {exc.lineno}', file=sys.stderr)
    print(f'    {line}', file=sys.stderr)
    if exc.offset is not None:
        print(f'    {" " * (exc.offset - number_of_leading_spaces)}^', file=sys.stderr)
    rprint(f'SyntaxError: {exc.msg}', file=sys.stderr)

def process_single_file(
    input_filename: str,
    file_content: str,
    output_filename: str|None,
    *,
    base_context: Context|None = None
) -> Context:
    if base_context is None:
        assert output_filename is not None, "output_file_name must be provided if there is no base_context"
        context = Context(input_filename, output_filename)
    else:
        context = Context.from_base_context(input_filename, base_context)
    template = create_template(input_filename, file_content, context)
    gen = CodeGenerator(input_filename)
    code = gen.compile(template)
    exec(code, context)
    return context

def process_file_handle_error(input_file: str, output_file: str) -> bool:
    input_file = os.path.abspath(input_file)
    output_file = os.path.abspath(output_file)
    init_importlib(os.path.dirname(input_file))

    try:
        with open(input_file, 'r') as f_in:
            content = f_in.read()
    except OSError as e:
        rprint(f'templ8: OSError: {e}', file=sys.stderr)
        return False

    try:
        context = process_single_file(input_file, content, output_file)
    except SyntaxError as psyntax:
        display_python_syntax_error(psyntax)
        return False
    except WSyntaxError as wsyntax:
        display_templating_syntax_error(wsyntax)
        return False
    except Exception:
        typ, exc, traceback = sys.exc_info()
        display_runtime_error(typ, exc, traceback)
        return False

    try:
        with open(output_file, 'w') as f_out:
            f_out.write(context.to_string())
    except OSError as e:
        rprint(f'templ8: OSError: {e}', file=sys.stderr)
        return False

    return True

class TemplateFileLoader(SourceFileLoader):
    def get_code(self, _):
        raise ImportError("importing templates via python is not allowed")

_missing = object()
def init_importlib(template_directory: str): 
    from importlib._bootstrap_external import FileFinder, _get_supported_file_loaders

    if getattr(sys, 'templates_cache', _missing) is not _missing:
        return

    if template_directory not in sys.path:
        sys.path.append(template_directory)
    sys.path_importer_cache.clear()
    prev_hook = next(filter(lambda x: 'FileFinder' in str(x), sys.path_hooks))
    sys.path_hooks.remove(prev_hook)

    supported_loaders = _get_supported_file_loaders()
    supported_loaders.append((TemplateFileLoader, ['.templ8']))
    sys.path_hooks.append(FileFinder.path_hook(*supported_loaders))
    sys.templates_cache = {}
