#!/usr/bin/env python

# generate some expressions to be used in tests

from typing import NamedTuple, Protocol
from random import randint, choice, random

ident_start = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
ident_next  = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
bin_ops   = "+-*/"
unary_ops = "+-"
whitespace = " \t\r\n\v"
expr_types = "buv0("

def gen_name():
    first = choice(ident_start)
    buf = [first]
    for _ in range(randint(0, 8)):
        buf.append(choice(ident_next))
    return ''.join(buf)

def gen_value():
    if random() < 0.1:
        return 0
    return randint(-0x8000_0000, 0x7fff_ffff)

def gen_whitespace():
    return ' '
    if randint(0, 1) == 0:
        return choice(whitespace)
    words = ' '.join(gen_name() for _ in range(randint(0, 5)))
    return f"# {words}\n"

# XXX: doesn't work right
def int32(x: int) -> int:
    x = x & 0xffffffff
    return (x ^ 0x80000000) - 0x80000000

class Node(Protocol):
    def eval(self, vars: dict[str, int]) -> int: ...

    def c_expr(self, vars: dict[str, int]) -> str: ...

class Paren(NamedTuple):
    child: Node

    def __str__(self):
        return f'({self.child})'

    def eval(self, vars: dict[str, int]) -> int:
        return self.child.eval(vars)
    
    def c_expr(self, vars: dict[str, int]) -> str:
        return f'({self.child.c_expr(vars)})'

class BinOp(NamedTuple):
    op: str
    lhs: Node
    rhs: Node

    def __str__(self):
        return f'{self.lhs}{gen_whitespace()}{self.op}{gen_whitespace()}{self.rhs}'

    def eval(self, vars: dict[str, int]) -> int:
        lhs = self.lhs.eval(vars)
        rhs = self.rhs.eval(vars)
        op = self.op

        if op == '+':
            value = lhs + rhs
        elif op == '-':
            value = lhs - rhs
        elif op == '*':
            value = lhs * rhs
        elif op == '/':
            value = lhs // rhs
        else:
            raise TypeError(f'illegal operator: {op}')

        return int32(value)

    def c_expr(self, vars: dict[str, int]) -> str:
        return f'{self.lhs.c_expr(vars)} {self.op} {self.rhs.c_expr(vars)}'


class Unary(NamedTuple):
    op: str
    child: Node

    def __str__(self):
        return f'{self.op}{gen_whitespace()}{self.child}'

    def eval(self, vars: dict[str, int]) -> int:
        value = self.child.eval(vars)
        if self.op == '-':
            value = int32(-value)
        return value

    def c_expr(self, vars: dict[str, int]) -> str:
        return f'{self.op} {self.child.c_expr(vars)}'

class Var(NamedTuple):
    name: str

    def __str__(self):
        return self.name

    def eval(self, vars: dict[str, int]) -> int:
        return vars.get(self.name, 0)
    
    def c_expr(self, vars: dict[str, int]) -> str:
        return str(vars.get(self.name, 0))

class Int(NamedTuple):
    value: int

    def __str__(self):
        return str(self.value)

    def eval(self, vars: dict[str, int]) -> int:
        return self.value

    def c_expr(self, vars: dict[str, int]) -> str:
        return str(self.value)

def gen_expr(vars: set[str]) -> Node:
    expr_type = choice(expr_types)
    if expr_type == 'b':
        op = choice(bin_ops)
        return BinOp(op, gen_expr(vars), gen_expr(vars))
    elif expr_type == 'u':
        op = choice(unary_ops)
        return Unary(op, gen_expr(vars))
    elif expr_type == 'v':
        var = gen_name()
        vars.add(var)
        return Var(var)
    elif expr_type == '0':
        return Int(gen_value())
    elif expr_type == '(':
        return Paren(gen_expr(vars))
    else:
        raise TypeError(f'illegal expression type: {expr_type}')

class TestCase(NamedTuple):
    expr: str
    environ: dict[str, int]
    parse_ok: bool
    result: str

    def __str__(self):
        buf: list[str] = ["(char*[]){"]
        for key, value in self.environ.items():
            buf.append(f'"{key}={value}", ')
        buf.append("NULL}")
        environ = ''.join(buf)
        str_parse_ok = 'true' if self.parse_ok else 'false'
        return f'{{ "{self.expr}", {str_parse_ok}, {environ}, {self.result} }}'

def gen_testcase():
    while True:
        vars: set[str] = set()
        environ: dict[str, int] = {}
        expr = gen_expr(vars)
        #print(expr)
        for var in vars:
            environ[var] = gen_value()
        str_expr = str(expr)
        parse_ok = True
        # TODO: negative examples
        if parse_ok:
            try:
                expr.eval(environ)
            except ZeroDivisionError:
                continue
        result = expr.c_expr(environ)
        return TestCase(
            str_expr, environ, parse_ok, result
        )

if __name__ == '__main__':
    for _ in range(100):
        test = gen_testcase()
        print(f'    {test},')
