import re
import inspect
from typing_extensions import Self

import Templ8.nodes as n
from typing import Type

class NodeVisitorMeta(type):
    def __new__(mcls, name, bases, attrs):
        if len(bases) > 1:
            raise TypeError('multiple inheritance with NodeVisitor is not allowed')
        if len(bases) > 0 and not inspect.isabstract(base := bases[0]):
            raise TypeError(f'{base.__name__} is sealed and cannot be subclassed')

        cls = type.__new__(mcls, name, bases, attrs)
        return cls

    def __init__(cls, name, bases, attrs):
        base = bases[0] if len(bases) == 1 else None
        supported_nodes = getattr(base, '__supported_nodes__', set())
        cls.__supported_nodes__: set[Type[n.Node]] = set(supported_nodes)
        abstracts = set()
        for method in getattr(base, '__abstractmethods__', set()):
            if getattr(cls, method, None) is None:
                abstracts.add(method)
        if len(abstracts) > 0:
            cls.__abstractmethods__ = abstracts

    def extend(cls, *nodes: Type[n.Node]) -> Self:
        if inspect.isabstract(cls):
            raise TypeError('extend expects a instantiable class')
        if len(nodes) == 0:
            raise TypeError('extend expects at least one Node')
        if any(inspect.isabstract(node) for node in nodes):
            raise TypeError('extend is not supported with abstract Nodes')
        cls = NodeVisitorMeta(cls.__name__, tuple(filter(lambda t: t != object, cls.__bases__)), dict(cls.__dict__))
        abstractmethods = set()
        for node in nodes:
            snake_case_name = '_'.join(p.lower() for p in re.findall('[A-Z][a-z]*', node.__name__))
            abstractmethods.add(f'visit_{snake_case_name}')
        setattr(cls, '__abstractmethods__', abstractmethods)
        cls.__supported_nodes__.update(nodes)
        return cls

class NodeVisitor(metaclass=NodeVisitorMeta):
    def visit(self, node: n.Node):
        if type(node) not in type(self).__supported_nodes__:
            raise TypeError(f'{node.__class__.__name__} is not supported by {self.__class__.__name__}')
        snake_case_name = '_'.join(p.lower() for p in re.findall('[A-Z][a-z]*', node.__class__.__name__))
        getattr(self, f'visit_{snake_case_name}')(node)

