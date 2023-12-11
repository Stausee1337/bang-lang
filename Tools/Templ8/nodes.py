from __future__ import annotations
import Templ8._nodes_meta as m

from Templ8.lexing import Pos
from types import NoneType

class Node(
    metaclass=m.NodeMeta,
    abstract=True
):
    start: Pos = m.field(kw_only=True)
    end:   Pos = m.field(kw_only=True)

class Block(Node):
    body: list[Node]

class Template(Node):
    body: list[Node]

class Expression(
    Node,
    abstract=True
):
    printable_name: str = m.static(init=False)

class Assign(
    Expression,
    printable_name='assignment' 
):
    lhs: Expression
    rhs: Expression

class Attribute(
    Expression,
    printable_name='attribute' 
):
    value: Expression
    attribute: str

class BinaryOp(
    Expression,
    printable_name='binary expression'
):
    left: Expression
    right: Expression
    operator: str

class CallArguments(
    Expression,
    printable_name='call arguments'
):
    args: list[Expression]
    kwargs: dict[str, Expression]

class Call(
    Expression,
    printable_name='call'
):
    function: Expression
    args: CallArguments

class Dict(
    Expression,
    printable_name='dict'
):
    items: dict[Expression, Expression]

class Lambda(
    Expression,
    printable_name='lambda'
):
    params: list[str]
    expression: Expression

class Literal(
    Expression,
    printable_name='literal'
):
    constant: int|str|bool|NoneType

class List(
    Expression,
    printable_name='list'
):
    items: list[Expression]

class Ternary(
    Expression,
    printable_name='ternary'
):
    condition: Expression
    true_branch: Expression
    false_branch: Expression

class Tuple(
    Expression,
    printable_name='tuple'
):
    items: list[Expression]

class Starred(
    Expression,
    printable_name='subscript'
):
    expr: Expression

class Subscript(
    Expression,
    printable_name='subscript'
):
    value: Expression
    slice: Expression

class UnaryOp(
    Expression,
    printable_name='unary expression'
):
    operator: str
    expr: Expression

class Variable(
    Expression,
    printable_name='variable'
):
    name: str

class Statement(
    Node,
    abstract=True
):
    pass

class Break(Statement):
    pass

class If(Statement):
    is_statement = True

    if_branches: list[tuple[Expression, Block]]
    else_branch: Block|None

class For(Statement):
    bound_vars: Expression
    iterator_expr: Expression 
    body: Block 

class Function(Statement):
    name: str
    args: list[str]
    body: list[Node] 

class Global(Statement):
    vars: list[str]

class Macro(Statement):
    name: str
    params: list[str]
    body: Block 

class Return(Statement):
    expr: Expression

class TryExcept(Statement):
    is_statement = True
    try_block: Block 
    exception_name: str
    except_block: Block

class While(Statement):
    condition: Expression
    body: Block

