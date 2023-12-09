import dis
import io
import Templ8.nodes as n

from Templ8.lexing import Pos
from Templ8.visitor import NodeVisitor

from types import CodeType
from rich import print as rprint
from rich.syntax import Syntax

INDENT_CHR   = ' '
INDENT_WIDTH = 4
INDENT       = INDENT_CHR * INDENT_WIDTH

class CodeGenerator(NodeVisitor.extend(
    n.Assign, n.Attribute, n.BinaryOp, n.Block, n.Break, n.Call, n.CallArguments,
    n.For, n.Function, n.If, n.Lambda, n.Literal, n.List, n.Macro, n.Subscript,
    n.Template, n.Tuple, n.Variable, n.While, n.UnaryOp, n.Dict, n.Return, n.Starred
)):
    def __init__(
        self,
        filename: str,
    ):
        self.filename = filename
        self.source = io.StringIO()
        self._indentation_level = 0
        self.source_code_lineno = 1
        self.linemap: dict[int, int] = {}
    
    def map_lineno(self, pos: Pos):
        if self.source_code_lineno not in self.linemap.keys():
            self.linemap[self.source_code_lineno] = pos.lineno + 1

    def write(self, content: str, /):
        for line in content.splitlines(keepends=True):
            self.source.write(line.rstrip('\n'))
            if line.endswith('\n'):
                self.linebreak()

    def indent(self):
        self._indentation_level += 1

    def dedent(self):
        assert self._indentation_level > 0, "dedent on level 0"
        self._indentation_level -= 1

    def linebreak(self):
        self.source.write('\n')
        self.source.write(INDENT * self._indentation_level)
        self.source_code_lineno += 1

    def blockvisit(self, nodes: list[n.Node], /):
        for node in nodes:
            self.visit(node)
            self.linebreak()

    def visit_assign(self, node: n.Assign):
        self.map_lineno(node.start)
        self.visit(node.lhs)
        self.write(' = ')
        self.visit(node.rhs)

    def visit_attribute(self, node: n.Attribute):
        self.map_lineno(node.start)
        self.visit(node.value)
        self.write(f'.{node.attribute}')

    def visit_binary_op(self, node: n.BinaryOp):
        self.map_lineno(node.start)
        self.visit(node.left)
        self.write(f' {node.operator} ')
        self.visit(node.right)

    def visit_break(self, _):
        self.write(f'break')

    def visit_block(self, node: n.Block):
        self.map_lineno(node.start)
        self.blockvisit(node.body)

    def visit_call(self, node: n.Call):
        self.map_lineno(node.start)
        self.visit(node.function)
        self.visit_call_arguments(node.args)

    def visit_call_arguments(self, node: n.CallArguments):
        self.map_lineno(node.start)
        self.write('(') #)
        for arg in node.args:
            self.visit(arg)
            self.write(', ')
        for name, kwarg in node.kwargs.items():
            self.write(f'{name}=')
            self.visit(kwarg)
            self.write(', ')
        self.write(')')

    def visit_dict(self, node: n.Dict):
        self.write('{') #}
        for key, value in node.items.items():
            self.write('(') #)
            self.visit(key)
            self.write('): ')
            self.write('(') #)
            self.visit(value)
            self.write('), ')
        self.write('}')

    def visit_function(self, node: n.Function):
        self.map_lineno(node.start)
        self.write(f'def {node.name}({", ".join(node.args)}):')
        self.indent()
        self.linebreak()
        self.write('pass\n')
        self.blockvisit(node.body)
        self.dedent()
        self.linebreak()

    def visit_for(self, node: n.For):
        self.map_lineno(node.start)
        self.write('for ')
        self.visit(node.bound_vars)
        self.write(' in ')
        self.visit(node.iterator_expr)
        self.write(' :')
        self.indent()
        self.linebreak()
        self.write('pass\n')
        self.visit_block(node.body)
        self.dedent()
        self.linebreak()

    def visit_while(self, node: n.While):
        self.map_lineno(node.start)
        self.write('while ')
        self.visit(node.condition)
        self.write(' :')
        self.indent()
        self.linebreak()
        self.write('pass\n')
        self.visit_block(node.body)
        self.dedent()
        self.linebreak()

    def visit_if(self, node: n.If):
        self.map_lineno(node.start)
        def write_branch(kind: str, condition: n.Expression|None):
            self.write(f'{kind} ')
            if condition is not None:
                self.visit(condition)
            self.write(':')

        self.linebreak()
        for idx, branch in enumerate(node.if_branches):
            write_branch('if' if idx == 0 else 'elif', branch[0])
            self.indent()
            self.linebreak()
            self.write('pass')
            self.linebreak()
            self.visit_block(branch[1])
            self.dedent()
            self.linebreak()

        if node.else_branch is not None:
            write_branch('else', None)
            self.indent()
            self.linebreak()
            self.write('pass')
            self.linebreak()
            self.visit_block(node.else_branch)
            self.dedent()

    def visit_lambda(self, node: n.Lambda):
        self.map_lineno(node.start)
        self.write('lambda ')
        if len(node.params) > 0:
            self.write(', '.join(node.params))
        self.write(' : ')
        self.visit(node.expression)

    def visit_literal(self, node: n.Literal):
        self.map_lineno(node.start)
        self.write(repr(node.constant))

    def visit_list(self, node: n.List):
        self.map_lineno(node.start)
        self.write('[') #]
        for expr in node.items:
            self.visit(expr)
            self.write(', ')
        self.write(']')

    def visit_macro(self, node: n.Macro):
        self.map_lineno(node.start)
        self.write('@macrodef\n')
        params = ', '.join(node.params)
        self.write(f'def {node.name}({params}):')
        self.indent()
        self.linebreak()
        self.write('pass\n')
        self.visit_block(node.body)
        self.dedent()
        self.linebreak()
        self.write(f'del {node.name}')

    def visit_starred(self, node: n.Starred):
        self.map_lineno(node.start)
        self.write('*')
        self.visit(node.expr)

    def visit_subscript(self, node: n.Subscript):
        self.visit(node.value)
        self.write('[') #]
        self.visit(node.slice)
        self.write(']')

    def visit_return(self, node: n.Return):
        self.write('return ')
        self.visit(node.expr)

    def visit_template(self, node: n.Template):
        self.blockvisit(node.body)

    def visit_tuple(self, node: n.Tuple):
        self.write('(') #)
        for expr in node.items:
            self.visit(expr)
            self.write(', ')
        self.write(')')

    def visit_variable(self, node: n.Variable):
        self.map_lineno(node.start)
        self.write(node.name)

    def visit_unary_op(self, node: n.UnaryOp):
        self.write(f'{node.operator} ')
        self.visit(node.expr)

    def compile(self, node: n.Node, *, mode='exec', _display=False) -> CodeType:
        self.visit(node)
        source = self.source.getvalue()
        if _display:
            print(f'{self.filename}:')
            rprint(Syntax(source, 'python', line_numbers=True)) 
        code = compile(source, self.filename, mode)
        return update_linetable(code, self.linemap) if len(self.linemap) > 0 else code
        return code

def update_linetable(code: CodeType, linemap: dict[int, int]) -> CodeType:
    consts = tuple(
        update_linetable(const, linemap) if isinstance(const, CodeType) else const for const in code.co_consts)
    fristlineno, linetable = build_linetable(code.co_firstlineno, code.co_linetable, linemap)
    return code.replace(
        co_name='<template>' if code.co_name == '<module>' else code.co_name,
        co_firstlineno=fristlineno,
        co_linetable=linetable, 
        co_consts=consts)

def build_linetable(firstlineno: int, blinetable: bytes, linemap: dict[int, int]) -> tuple[int, bytes]:
    niter = iter(list(blinetable))
    linetable = [(nbytes, next(niter)) for nbytes in niter]
    new_linetable = []

    current = firstlineno
    prev_template = linemap[current]
    current_nbytes = 0
    for nbytes, increment in linetable:
        current += increment
        new_template = linemap.get(current, prev_template)
        if new_template != prev_template:
            new_linetable.append((current_nbytes, prev_template))
            current_nbytes = 0
        current_nbytes += nbytes
        prev_template = new_template
    new_linetable.append((current_nbytes, prev_template))

    current = linemap[firstlineno]
    result = bytearray()
    for nbytes, abslineno in new_linetable:
        result.append(nbytes)
        result.append(abslineno - current)
        current = abslineno

    return (linemap[firstlineno], bytes(result))

