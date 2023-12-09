import Templ8.nodes as n
from Templ8.lexing import Pos, expect, find_next, test, tokenize_line, unexpected_error, TokenStream, Token, WSyntaxError

def ensure_arguments(expr: n.Expression|list[n.Expression]|dict[str, n.Expression]) -> n.CallArguments:
    match expr:
        case n.CallArguments():
            return expr
        case n.Tuple():
            return n.CallArguments(expr.items, {}, start=expr.start, end=expr.end)
        case list():
            return n.CallArguments(expr, {}, start=expr[0].start, end=expr[-1].end)
        case dict():
            vals = list(expr.values())
            return n.CallArguments([], expr, start=vals[0].start, end=vals[-1].end)
        case _:
            return n.CallArguments([expr], {}, start=expr.start, end=expr.end)

def parse_format_string(token: Token) -> n.Expression:
    fstring = token.value[:-1]
    items: list[n.Expression] = []

    pos = 1
    while True:
        if (result := find_next(fstring, pos, '{', '}')) is None: #}
            break
        chr, idx = result
        rest = fstring[idx+1:]
        if chr == '}':
            if not rest.startswith('}'):
                raise WSyntaxError((token.start + idx, token.filename), 'Unexpected end of in-fstring-expression; If you want to put `}`-char use `}}` escaping')
                print(f'{(token.start + idx).here(token.filename)} ERROR: Unexpected end of in-fstring-expression; If you want to put `}}`-char use `}}}}` escaping')
                exit(1)
            items.append(n.Literal(
                fstring[pos:idx+1].encode().decode('unicode_escape'),
                start=token.start + pos,
                end=token.start + idx + 1
            ))
            pos = idx + 2
            continue
        if rest.startswith('{'): # }
            items.append(n.Literal(
                fstring[pos:idx+1].encode().decode('unicode_escape'),
                start=token.start + pos,
                end=token.start + idx + 1
            ))
            pos = idx + 2
            continue

        string_part = fstring[pos:idx]

        level = 0
        sidx = 0
        start_pos = idx + 1
        for sidx, chr in enumerate(rest):
            if chr == '{': #}
                level += 1
            if chr == '}':
                if level == 0:
                    break
                level -= 1
        else:
            raise WSyntaxError((token.start + start_pos + sidx, token.filename), 'Format string closed unexpectedly during expression')
            print(f'{(token.start + start_pos + sidx).here(token.filename)} ERROR: Format string closed unexpectedly during expression')
            exit(1)
        
        if string_part != '':
            items.append(n.Literal(
                string_part.encode().decode('unicode_escape'),
                start=token.start + pos,
                end=token.start + idx
            ))

        stream = tokenize_line(
            token.filename,
            fstring[start_pos:start_pos + sidx],
            token.start + start_pos)
        next(stream)
        expr = parse_tuple(stream)
        items.append(n.Call(
            n.Variable('str',
                start=(invalipos := Pos(token.start.lineno, -1)),
                end=invalipos),
            ensure_arguments(expr),
            start=invalipos, end=invalipos
        ))

        pos = start_pos + sidx + 1

    if (t := fstring[pos:]) != '':
        items.append(n.Literal(t.encode().decode('unicode_escape'), start=token.start + pos, end=token.end))

    invalipos = Pos(token.start.lineno, -1)
    return n.Call(
        n.Attribute(
            n.Variable('str', start=invalipos, end=invalipos),
            'join',
            start=invalipos, end=invalipos),
        ensure_arguments([
            n.Literal('', start=invalipos, end=invalipos),
            n.List(items, start=invalipos, end=invalipos)]),
        start=token.start, end=token.end
    )

def parse_primary(stream: TokenStream) -> n.Expression:
    token = stream.current
    match token.kind:
        case 'ident':  # variable
            if token.value in ('True', 'False'):
                return n.Literal(token.value == 'True', **token)
            if token.value == 'None':
                return n.Literal(None, **token)
            if token.value == 'f' and test((strl := stream.lookahead(1)), 'str:'):
                assert strl is not None, "Unreachable"
                next(stream)
                return parse_format_string(strl)
            if not token.value.isidentifier():
                unexpected_error(token, 'valid python identfier')
            return n.Variable(token.value, **token)
        case 'num' | 'str':
            return n.Literal(
                int(token.value) if token.kind == 'num' else token.value[1:-1].encode().decode('unicode_escape'),
                **token)
        case 'punct':
            if token.value == '(':  # )
                return parse_tuple(stream, explicit_parentheses=True)
            elif token.value == '{':  # }:
                return parse_binary(stream)
            elif token.value == '[': # ]
                return parse_list(stream)

    unexpected_error(token, 'either string, fstring, number, boolean, variable, tuple, list or opset')
    assert False, "unreachable"

def parse_list(stream: TokenStream) -> n.List:
    assert test(stream.current, 'punct:[') # ]
    next(stream)

    items: list[n.Expression] = []

    empty_list = False
    if test((starttok := stream.current), 'punct:]'):
        empty_list = True

    while not empty_list:
        expr = parse_expr(stream)
        items.append(expr)
        if not test(stream.current, 'punct:,'):
            break
        next(stream)

    if not test((endtok := stream.current), 'punct:]'):
        unexpected_error(stream.current, 'closing bracket')

    return n.List(items, start=starttok.start, end=endtok.end)

def parse_binary(stream: TokenStream) -> n.BinaryOp:
    assert test((starttok := stream.current), 'punct:{') # }
    next(stream)

    if test((token := stream.current), 'punct:}'):
        raise WSyntaxError(token, 'Empty Set {} is invalid; Operator Set must have exactly two elements')
        print(f'{token.start.here(token.filename)} ERROR: Empty Set {{}} is invalid; Operator Set must have exactly two elements')
        exit(1)

    left = parse_expr(stream)
    if test((token := stream.current), 'punct:}'):
        raise WSyntaxError(token, 'Only LHS Set {<expr>} is invalid; Operator Set must have exactly two elements')
        print(f'{token.start.here(token.filename)} ERROR: Only LHS Set {{<expr>}} is invalid; Operator Set must have exactly two elements')
        exit(1)
    right = parse_expr(stream)

    if not test((token := stream.current), 'punct:}'):
        raise WSyntaxError(token, 'Op Set has exactly two elments {<lhs> <rhs>}<op>')
        print(f'{token.start.here(token.filename)} ERROR: Op Set has exactly two elments {{<lhs> <rhs>}}<op>')
        exit(1)
    next(stream)

    if not test(stream.current, 'punct:'):
        unexpected_error(stream.current, f'valid operator; one of {", ".join(valid_ops)}')

    operator = stream.current.value
    two_punct_op = operator
    if test((ll := stream.lookahead(1)), 'punct:'):
        assert ll is not None, "Unreachable"
        two_punct_op += ll.value
        if two_punct_op in valid_ops:
            operator = two_punct_op
            next(stream)

    if operator not in valid_ops:
        unexpected_error(stream.current, f'valid operator; one of {", ".join(valid_ops)}')

    return n.BinaryOp(left, right, operator, start=starttok.start, end=stream.current.end)

def parse_unary(stream: TokenStream) -> Token|None:
    token = stream.current
    if test(token, 'punct:+ - ~ !'):
        next(stream)
        return token

def parse_postfix(stream: TokenStream) -> n.Expression:
    def inner(base: n.Expression) -> n.Expression|None:
        token = stream.current
        if test(token, 'punct:.'):  # member expression:
            member = expect(stream, 'ident:').value
            if not member.isidentifier():
                unexpected_error(token, 'valid python identfier')
            return n.Attribute(base, member, start=base.start, end=stream.current.end)
        elif test(token, 'punct:['):  # ] subscript expression
            next(stream)
            expr = n.Subscript(base, parse_tuple(stream), start=base.start, end=stream.current.end)
            if not test(stream.current, 'punct:]'):
                print(stream.current)
                unexpected_error(stream.current, 'closing bracket')
            return expr
        return None

    expr = parse_primary(stream)
    while not stream.is_eof():
        next(stream)
        if (result := inner(expr)) is None:
            return expr
        expr = result
    return expr

def parse_call(stream: TokenStream) -> n.Expression:
    op = parse_unary(stream)
    expr = parse_postfix(stream)
    if op is not None:
        expr = n.UnaryOp(op.value, expr, start=op.start, end=expr.end)
    call_chain: list[n.Expression] = [expr]

    while not stream.is_eof():
        if not test(stream.current, 'punct:|'):
            break
        next(stream)
        expr = parse_postfix(stream)
        call_chain.append(expr)

    if len(call_chain) == 1:
        return expr

    arg = call_chain.pop(0)
    for call in call_chain:
        arg = n.Call(call, ensure_arguments(arg), start=arg.start, end=call.end)

    return arg

valid_ops = {
    '==', '!=', '>', '>=', '<', '<=', '?', '!?',
    '+', '-', '*', '/'
}

def is_expression_starrable(expr: n.Expression):
    pass

def parse_expr(stream: TokenStream) -> n.Expression:
    if (is_starred := test((startok := stream.current), 'punct:*')):
        next(stream)

    expr = parse_call(stream)
    if not test(stream.current, 'punct:?'):
        if is_starred:
            is_expression_starrable(expr)
            return n.Starred(expr, start=startok.start, end=expr.end)
        return expr
    if is_starred:
        raise WSyntaxError(stream.current, 'ternary is invalid after starred expression')
        print(
            stream.current.start.here(stream.filename) +
            f' ERROR: ternary is invalid after starred expression')
        exit(1)
    next(stream)
    true_branch = parse_expr(stream)
    if isinstance(true_branch, n.Starred):
        raise WSyntaxError((true_branch.start, stream.filename), 'starred expression is invalid within ternary')
        print(
            true_branch.start.here(stream.filename) +
            f' ERROR: starred expression is invalid within ternary')
        exit(1)
    if not test(stream.current, 'punct::'):
        unexpected_error(stream.current, 'colon')
        assert False, "Unreachable"
    next(stream)

    false_branch = parse_expr(stream)
    if isinstance(false_branch, n.Starred):
        raise WSyntaxError((false_branch.start, stream.filename), 'starred expression is invalid within ternary')
        print(
            false_branch.start.here(stream.filename) +
            f' ERROR: starred expression is invalid within ternary')
        exit(1)
    return n.Ternary(expr, true_branch, false_branch, start=expr.start, end=false_branch.end)

def parse_tuple(stream: TokenStream, *, explicit_parentheses=False) -> n.Expression:
    tuple_expression: list[n.Expression] = []

    starttok = stream.current
    
    empty_tuple = False
    if explicit_parentheses:
        assert test(stream.current, 'punct:('), 'Calling parse_tuple(explicit_parentheses=True), requires a lparen as current token'  # ) 
        empty_tuple = test(next(stream), 'punct:)')

    explicit_one_len_tuple = False
    while not stream.is_eof() and not empty_tuple:
        expr = parse_expr(stream)
        if not test(stream.current, 'punct:,'):
            if len(tuple_expression) > 0 or explicit_parentheses:
                tuple_expression.append(expr)
                break
            return expr
        if explicit_parentheses and test(stream.current, 'punct:,') and test(stream.lookahead(1), 'punct:)'):
            tuple_expression.append(expr)
            next(stream)
            explicit_one_len_tuple = True
            break
        tuple_expression.append(expr)
        next(stream)

    if explicit_parentheses:
        start = starttok.start
        end = stream.current.end
    else:
        start = tuple_expression[0].start
        end = tuple_expression[-1].end

    expr = tuple_expression[0] if len(tuple_expression) == 1 and not explicit_one_len_tuple else n.Tuple(tuple_expression, start=start, end=end)
    if not explicit_parentheses:
        return expr

    if not test(stream.current, 'punct:)'):
        unexpected_error(stream.current, 'closing paren')

    return expr

def is_valid_lhs(filename: str, expr: n.Expression):
    if isinstance(expr, (n.Variable, n.Attribute, n.Subscript)):
        return
    elif isinstance(expr, (n.Tuple, n.List)):
        for item in expr.items:
            is_valid_lhs(filename, item)
    elif isinstance(expr, n.Starred):
        is_valid_lhs(filename, expr.expr)
    else:
        raise WSyntaxError((expr.start, filename), f'{expr.printable_name} is invalid LHS for assignment')
        print(
            expr.start.here(filename) +
            f' ERROR: {expr.printable_name} is invalid LHS for assignment')
        exit(1)

def parse_assign_lhs(stream: TokenStream) -> n.Expression:
    lhs = parse_tuple(stream)
    is_valid_lhs(stream.filename, lhs)
    return lhs

