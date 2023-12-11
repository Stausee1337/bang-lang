from dataclasses import dataclass
import functools
import re
from typing import Callable, Generator, Iterator, List, Mapping, ParamSpec
from typing_extensions import Self

TOK_INITIAL = "init"
TOK_IDENTIFIER = "ident"
TOK_NUMBER = "num"
TOK_PUCTUATOR = "punct"
TOK_COMMENT = "comment"
TOK_STRING = "str"
TOK_EOF = "eof"

SUPPORTED_TOKENS = {TOK_IDENTIFIER, TOK_NUMBER, TOK_PUCTUATOR, TOK_STRING, TOK_EOF}
register_tokens = lambda *x: SUPPORTED_TOKENS.update(x)

@dataclass(frozen=True)
class Pos:
    lineno: int
    col: int

    def __add__(self, other: int) -> Self:
        return Pos(self.lineno, self.col + other)

    def __sub__(self, other: int) -> Self:
        return Pos(self.lineno, self.col - other)

    def here(self):
        return f'{self.lineno+1}:{self.col+1}'

@dataclass(unsafe_hash=True)
class Token(Mapping[str, Pos]):
    kind: str
    value: str
    start: Pos
    end: Pos
    filename: str

    def __len__(self) -> int:
        return 2
    
    def __iter__(self) -> Iterator[str]:
        yield from ('start', 'end')

    def __getitem__(self, key: str) -> Pos:
        if key not in ('start', 'end'):
            raise KeyError(key)
        return getattr(self, key)
    
    def __repr__(self) -> str:
        return f'{self.kind}:{self.value}'

punctuation = r"""!$%&@()*+,-./:;<=>?[\]^{|}~"""
PUNCTS = re.escape(punctuation)
STRING = r"(?:'[^\n'\\]*(?:\\.[^\n'\\]*)*')"

class WSyntaxError(Exception):
    def __init__(self, location: Token|tuple[Pos, str], message: str):
        self.location = location
        super().__init__(message)

    def position(self) -> Pos:
        if isinstance(self.location, tuple):
            return self.location[0]
        return self.location.start
    
    def filename(self) -> str:
        if isinstance(self.location, tuple):
            return self.location[1]
        return self.location.filename

class TokenStream:
    def __init__(self, init: Token, stream: Iterator[Token]):
        self._stream = stream
        self.filename = init.filename
        self.emit_eof = True
        self.forked_from: None|Self = None
        self.buffer = init.value

        self._current = init
        self._cached: None|Token = None
        self._lookahead: List[Token] = []
        self._buf_last_lno = init.start.lineno
        self._last_col = 0

    def is_eof(self):
        return self._current.kind == TOK_EOF and len(self._lookahead) == 0

    def get_text_slice(self, start: Pos, end: Pos) -> str:
        if self.forked_from is not None:
            return self.forked_from.get_text_slice(start, end)
        lines = self.buffer.splitlines(keepends=True)
        sliced_lines = lines[start.lineno:end.lineno+1]
        sliced_lines[0] = sliced_lines[0][start.col:]
        one_line_diff = 0 if start.lineno != end.lineno else start.col
        sliced_lines[-1] = sliced_lines[-1][:end.col - one_line_diff]
        return ''.join(sliced_lines)

    @property
    def current(self) -> Token:
        if self._cached is not None:
            return self._cached
        return self._current
    
    def __iter__(self):
        return self
    
    def fork_limited(self, *, eof_detect: str|Callable[[Self], bool], skip_init=True, relative_fork=False) -> Self:
        if isinstance(eof_detect, str):
            new_eof = eof_detect
            eof_detect = lambda s: test(s.current, new_eof)

        middle_streams = []
        if self.forked_from is None or relative_fork:
            parent = self
        else:
            parent = recursively_find_root(self, middle_streams)

        stream = _limited_generator(parent, eof_detect, middle_streams=middle_streams)
        stream.forked_from = self
        if skip_init:
            next(stream)
        return stream

    def lookahead(self, ll: int) -> Token|None:
        assert ll > 0, 'lookahead 0 is not a thing, just use TokenStream.current'
        if ll <= len(self._lookahead):
            return self._lookahead[ll - 1]

        tok = None
        for _ in range(ll - len(self._lookahead)):
            tok = self._preform_lookahead()
        return tok

    def _preform_lookahead(self) -> Token|None:
        if self._current.kind == TOK_EOF:
            return None
        if self._cached is None:
            self._cached = self._current
        token = self.__next__(lookahead=True)
        self._lookahead.append(token)
        return token
    
    def __next__(self, *, lookahead=False) -> Token:
        if self.is_eof():
            raise StopIteration 
        if len(self._lookahead) > 0 and not lookahead:
            token = self._lookahead.pop(0)
            self._cached = None
            self._current = token
            if token.kind == TOK_EOF and not self.emit_eof:
                raise StopIteration
            return token

        try:
            self._current = next(self._stream)
            self._last_col = self._current.end.col
        except StopIteration:
            self._current = Token(TOK_EOF, '', (start := Pos(self._buf_last_lno, self._last_col)), start, self.filename)
            if not self.emit_eof and not lookahead:
                raise StopIteration
        
        return self._current

class LimitedException(Exception):
    def __init__(self):
        super().__init__(
            "Limited iterator: end of stream was reached, before declared EOF token; this is undefined behaviour")

def recursively_find_root(current: TokenStream, middle_streams: list[TokenStream]) -> TokenStream:
    while current.forked_from is not None:
        middle_streams.append(current)
        current = current.forked_from
    return current

Param = ParamSpec("Param")

def tokenwraps(func: Callable[Param, Generator[Token, None, None]]) -> Callable[Param, TokenStream]:
    @functools.wraps(func)
    def inner(*args: Param.args, **kwargs: Param.kwargs) -> TokenStream:
        stream = func(*args, **kwargs)
        init = next(stream)
        return TokenStream(init, stream)

    return inner

@tokenwraps
def _limited_generator(stream: TokenStream, check_eof: Callable[[TokenStream], bool], *, middle_streams: list[TokenStream]) -> Generator[Token, None, None]:
    token = stream.current
    yield Token(TOK_INITIAL, '', token.start, token.start, token.filename)
    while not stream.is_eof():
        if check_eof(stream):
            break
        yield token
        token = next(stream) 
        for mstream in middle_streams:
            mstream._current = token
    else:
        raise LimitedException

@tokenwraps
def tokenize_line(fname: str, line: str, pos: Pos) -> Generator[Token, None, None]:
    lineno, col = pos.lineno, pos.col
    def create_tok(kind: str) -> Token:
        return Token(kind, c, (start := Pos(lineno, col)) + m.start(), start + m.end(), fname)

    yield Token(TOK_INITIAL, line, (start := Pos(lineno, 0)), start, fname)
    ucol = col
    for m in re.finditer(fr'(\s+|\w+|//.*|\d+|{STRING}|[{PUNCTS}])', line.rstrip()):
        c: str = m.group(1)
        if c.isspace(): 
            ucol = col + m.end()
            continue
        elif c.isidentifier():
            yield create_tok(TOK_IDENTIFIER)
            # ucol = col + m.end()
        elif c[0].isdigit():
            yield create_tok(TOK_NUMBER)
            # ucol = col + m.end()
        elif c[:2] == '//':
            yield create_tok(TOK_COMMENT)
            # ucol = col + m.end()
        elif c[0] == "'":
            yield create_tok(TOK_STRING)
            # ucol = col + m.end()
        elif c[0] in punctuation:
            yield create_tok(TOK_PUCTUATOR)
            # ucol = col + m.end()
        else:
            raise WSyntaxError((Pos(lineno, ucol-1), fname), f'unexpected character {line[ucol]!r}')
            print(f'{fname}:{lineno+1}:{ucol} ERROR: unexpected character {line[ucol]!r}')
            exit()
        ucol = col + m.end()

@tokenwraps
def tokenize(fname: str, buf: str, *, lineno: int = 0, col: int = 0) -> Generator[Token, None, None]:
    startlineno = lineno
    startcol = col
    lines = buf.splitlines()
    yield Token(TOK_INITIAL, buf, (start := Pos(len(lines) - 1, 0)), start, fname)
    for lineno, line in enumerate(lines):
        substream = tokenize_line(fname, line, Pos(lineno + startlineno, startcol))
        substream.emit_eof = False
        yield from substream

def find_next(contents: str, start: int, *args: str) -> tuple[str, int]|None:
    for idx in range(start, len(contents)):
        for p in args:
            if contents.startswith(p, idx):
                return p, idx

def test(token: Token|None, pattern: str) -> bool:
    if token is None:
        return False
    skind, svalue = pattern.split(':', maxsplit=1)
    if ' ':
        kinds = skind.split()
    else:
        kinds = [skind]
    if ' ' in svalue:
        values = svalue.split()
    else:
        values = [svalue]
    if any(kind not in SUPPORTED_TOKENS for kind in kinds):
        unsupported = set(kinds) - SUPPORTED_TOKENS
        raise RuntimeError(f'test(): Unsupported kind {", ".join(repr(u) for u in unsupported)}')
    if token.kind in kinds:
        if svalue == '':
            return True
        if token.value in values:
            return True
    return False

def unexpected_error(token: Token, expected: str|None, *, location: Pos|None = None):
    if token.kind != TOK_EOF:
        msg = f'Found {token.kind}:{token.value}, expected {expected}' if expected is not None else f'Unexpected {token.kind}'
    else:
        assert expected is not None
        msg = f'Expected {expected}'
    if location is None:
        raise WSyntaxError(token, msg)
    raise WSyntaxError((location, token.filename), msg)

def expect(stream: Iterator[Token], pattern: str) -> Token:
    token = next(stream)
    if test(token, pattern):
        return token
    kind, value = pattern.split(':', maxsplit=1)
    unexpected_error(token, f'{kind}:{value}')
    assert False, "unreachable"

