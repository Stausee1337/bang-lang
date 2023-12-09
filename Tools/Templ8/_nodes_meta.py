import inspect
from types import FunctionType
from typing import Any, Callable, Literal
from typing_extensions import dataclass_transform

def class_constructor(positional: list[str], keyword: dict[str, Any]) -> tuple[str, str]:
    INDENT = ' ' * 4
    p = len(positional) > 0
    k = len(keyword) > 0
    a = len(positional) + len(keyword) > 0
    body = (f"self.{attr} = {attr}" for attr in positional + list(keyword.keys()))
    kwarg_str = (", *, " if p else "*, ") + ", ".join(arg if default is _missing else f"{arg}={default!r}" for arg, default in keyword.items())
    source = f'def __init__(self, {", ".join(positional) if p else ""}{kwarg_str if k else ""}):\n{INDENT}{(chr(0x0a) + INDENT).join(body) if a else "pass"}'
    return '__init__', source

def pysubclass_initializer() -> tuple[str, str]:
    source = '''\
def __init_subclass__(cls, **kwargs):
    if not getattr(cls, '__abstract__', False):
        cls.__meta_init_subclass__(**kwargs)
    else:
        super(cls).__init_subclass__()\
'''
    return '__init_subclass__', source

def custom_subclass_initializer(statics: dict[str, Any]) -> tuple[str, str]:
    assert len(statics) > 0
    INDENT = ' ' * 4
    kwarg_str = ', '.join(arg if default is _missing else f"{arg}={default!r}" for arg, default in statics.items())
    body_str = f'\n{INDENT}'.join(f'cls.{attr} = {attr}' for attr in statics.keys())
    source = f'@classmethod\ndef __meta_init_subclass__(cls, *, {kwarg_str}, **kwargs):\n{INDENT}{body_str}'
    source += f'\n{INDENT}super(cls).__init_subclass__(**kwargs)'
    return '__meta_init_subclass__', source

def class_display(attrs: list[str]) -> tuple[str, str]:
    INDENT = ' ' * 4
    dict_str = 'dict(' + ', '.join(f'{attr}=self.{attr}' for attr in attrs) + ')'
    content_str = f"', '.join('%s=%r' % (k, v) for k, v in {dict_str}.items())"
    body_str = (
        (f'{INDENT}content = {f"repr(self.{attrs[0]})" if len(attrs) == 1 else content_str}\n' if len(attrs) > 0 else '') +
        f"{INDENT}return '%s({'%s' if len(attrs) > 0 else ''})' % (self.__class__.__name__, {'content' if len(attrs) > 0 else ''})")
    source = f'def __repr__(self):\n{body_str}'
    return '__repr__', source

def build_method(modname: str, classname: str, source_generator: Callable[..., tuple[str, str]]) -> FunctionType:
    fn_name, script_code = source_generator()
    fullname = f'{modname}.{classname}'
    result = compile(script_code, f'<metaclass generated {fn_name} code for {fullname!r}>', 'exec')
    glob = {}
    glob.update(globals())
    exec(result, glob, glob)

    init_fn = glob[fn_name]
    init_fn.__qualname__ = f'{classname}.__init__'
    return init_fn

def extract_defined_attrs(attrs: dict[str, Any]) -> tuple[list[str], dict[str, Any], dict[str, Any], dict[str, Any]]:
    annotations = attrs.get('__annotations__', {})
    args = list(annotations.keys())
    kwargs = {}
    statics = {}

    for arg in args[:]:
        if arg not in attrs.keys():
            continue
        args.remove(arg)
        if (decorator := attrs[arg]) and isinstance(decorator, _Decorator) and decorator.kw_only: 
            kwargs[arg] = decorator.default
        if (decorator := attrs[arg]) and isinstance(decorator, _Decorator) and decorator.static:
            statics[arg] = decorator.default
            if decorator.default is _missing:
                del attrs[arg]
            else:
                attrs[arg] = decorator.default
        else:
            del attrs[arg]
        
    return args, kwargs, statics, annotations

class _Decorator:
    def __init__(self, *, kw_only: bool, static: bool, default: Any):
        self.kw_only = kw_only
        self.static = static
        self.default = default

_missing = object()
def field(*, kw_only=False, default=_missing) -> Any:
    return _Decorator(kw_only=kw_only, static=False, default=default)

def static(*, init: Literal[False], default=_missing) -> Any:
    assert not init, "Init needs to be there for type checking reasons; but for static it must always be False"
    return _Decorator(kw_only=False, static=True, default=default)

@dataclass_transform(field_specifiers=(field, static))
class NodeMeta(type):
    def __new__(mcls, name, bases, attrs, abstract=False, **kwargs):
        base = None
        if len(bases) > 1:
            raise TypeError('multiple inheritance with Node is not allowed')
        if len(bases) > 0 and not inspect.isabstract(base := bases[0]):
            raise TypeError(f'{base.__name__} is sealed and cannot be subclassed')
        attr_args, attr_kwargs, attr_statics, annotations = extract_defined_attrs(attrs)
        if abstract:
            if len(attr_args) > 0:
                type_annotation = f': {annotations.get(attr_args[0])}' if attr_args[0] in annotations else ''
                raise TypeError(f'abstract classes only supported kwarg attributes; use `{attr_args[0]}{type_annotation} = decorator(kw_only=True)`')
            attrs['__abstract__'] = True 
            keyword_args = {}
            keyword_args.update(getattr(base, '__attrs__', {}))
            keyword_args.update(attr_kwargs)
            attrs['__attrs__'] =  keyword_args
            if len(attr_statics) > 0:
                attrs['__init_subclass__'] = build_method(attrs['__module__'], name, lambda: pysubclass_initializer())
                attrs['__meta_init_subclass__'] = build_method(attrs['__module__'], name, lambda: custom_subclass_initializer(attr_statics))
            attrs['__slots__'] = ()
        else:
            if len(attr_kwargs) > 0:
                raise TypeError('kwarg attributes are only supported in abstract classes')
            if len(attr_statics) > 0:
                raise TypeError('static class initializer attributes are only supported in abstract classes')
            keyword_args = getattr(base, '__attrs__', {})
            # list(attrs.get('__annotations__', {}).keys())
            attrs['__init__'] = build_method(attrs['__module__'], name, lambda: class_constructor(attr_args, keyword_args))
            attrs['__repr__'] = build_method(attrs['__module__'], name, lambda: class_display(attr_args))
            fields = tuple(attr_args + list(keyword_args.keys()))
            attrs['__slots__'] = fields
        cls = type.__new__(mcls, name, bases, attrs, **kwargs)
        if abstract:
            delattr(cls, '__abstract__')
            setattr(cls, '__abstractmethods__', {'__init__'})
        return cls
