#!/usr/bin/env python

# generate some expressions to be used in tests

from typing import NamedTuple, Protocol
from random import randint, choice, random

ident_start = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
ident_next  = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
bin_ops   = ["+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&", "|", "^", "&&", "||", "<<", ">>"]
unary_ops = "+-~!"
whitespace = " \t\r\n\v"
non_terneary = "buv0("
expr_types = "buv0(?"
leaf_types = "v0"

def gen_name():
    first = choice(ident_start)
    buf = [first]
    for _ in range(randint(0, 8)):
        buf.append(choice(ident_next))
    return ''.join(buf)

def gen_value(min: int = -0x8000_0000, max: int = 0x7fff_ffff):
    if 0 >= min and 0 <= max and random() < 0.1:
        return 0
    return randint(min, max)

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

class If(NamedTuple):
    cond: Node
    then_expr: Node
    else_expr: Node

    def __str__(self):
        return f'{self.cond}{gen_whitespace()}?{gen_whitespace()}{self.then_expr}{gen_whitespace()}:{gen_whitespace()}{self.else_expr}'

    def eval(self, vars: dict[str, int]) -> int:
        cond = self.cond.eval(vars)
        then_res = self.then_expr.eval(vars)
        else_res = self.else_expr.eval(vars)
        return then_res if cond else else_res

    def c_expr(self, vars: dict[str, int]) -> str:
        return f'{self.cond.c_expr(vars)} ? {self.then_expr.c_expr(vars)} : {self.else_expr.c_expr(vars)}'

class BinOp(NamedTuple):
    op: str
    lhs: Node
    rhs: Node

    def __str__(self):
        return f'{self.lhs}{gen_whitespace()}{self.op}{gen_whitespace()}{self.rhs}'

    def eval(self, vars: dict[str, int]) -> int:
        op = self.op

        if op == '+':
            value = self.lhs.eval(vars) + self.rhs.eval(vars)
        elif op == '-':
            value = self.lhs.eval(vars) - self.rhs.eval(vars)
        elif op == '*':
            value = self.lhs.eval(vars) * self.rhs.eval(vars)
        elif op == '/':
            value = self.lhs.eval(vars) // self.rhs.eval(vars)
        elif op == '%':
            value = self.lhs.eval(vars) % self.rhs.eval(vars)
        elif op == '^':
            value = self.lhs.eval(vars) ^ self.rhs.eval(vars)
        elif op == '|':
            value = self.lhs.eval(vars) | self.rhs.eval(vars)
        elif op == '&':
            value = self.lhs.eval(vars) & self.rhs.eval(vars)
        elif op == '&&':
            lhs = self.lhs.eval(vars)
            rhs = self.rhs.eval(vars)
            value = int(lhs and rhs)
        elif op == '||':
            lhs = self.lhs.eval(vars)
            rhs = self.rhs.eval(vars)
            value = int(lhs or rhs)
        elif op == '<':
            value = int(self.lhs.eval(vars) < self.rhs.eval(vars))
        elif op == '>':
            value = int(self.lhs.eval(vars) > self.rhs.eval(vars))
        elif op == '<=':
            value = int(self.lhs.eval(vars) <= self.rhs.eval(vars))
        elif op == '>=':
            value = int(self.lhs.eval(vars) >= self.rhs.eval(vars))
        elif op == '==':
            value = int(self.lhs.eval(vars) == self.rhs.eval(vars))
        elif op == '!=':
            value = int(self.lhs.eval(vars) != self.rhs.eval(vars))
        elif op == '<<':
            rhs = self.rhs.eval(vars)
            if rhs > 32 or rhs < 0:
                raise ValueError('shift out of range')
            value = int(self.lhs.eval(vars) << rhs)
        elif op == '>>':
            rhs = self.rhs.eval(vars)
            if rhs > 32 or rhs < 0:
                raise ValueError('shift out of range')
            value = int(self.lhs.eval(vars) >> rhs)
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
        elif self.op == '!':
            value = int(not value)
        elif self.op == '~':
            value = ~value
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

def gen_expr_old(vars: set[str]) -> Node:
    expr_type = choice(expr_types)
    if expr_type == 'b':
        op = choice(bin_ops)
        return BinOp(op, gen_expr_old(vars), gen_expr_old(vars))
    elif expr_type == 'u':
        op = choice(unary_ops)
        return Unary(op, gen_expr_old(vars))
    elif expr_type == 'v':
        var = gen_name()
        vars.add(var)
        return Var(var)
    elif expr_type == '0':
        return Int(gen_value())
    elif expr_type == '(':
        return Paren(gen_expr_old(vars))
    elif expr_type == '?':
        return If(gen_expr_old(vars), gen_expr_old(vars), gen_expr_old(vars))
    else:
        raise TypeError(f'illegal expression type: {expr_type}')

def gen_expr(vars: set[str], size: int) -> Node:
    def gen_expr():
        nonlocal size
        size -= 1

        if size > 2:
            expr_type = choice(expr_types)
        elif size > 1:
            expr_type = choice(non_terneary)
        else:
            expr_type = choice(leaf_types)

        if expr_type == 'b':
            op = choice(bin_ops)
            return BinOp(op, gen_expr(), gen_expr())
        elif expr_type == 'u':
            op = choice(unary_ops)
            return Unary(op, gen_expr())
        elif expr_type == 'v':
            var = gen_name()
            vars.add(var)
            return Var(var)
        elif expr_type == '0':
            return Int(gen_value())
        elif expr_type == '(':
            return Paren(gen_expr())
        elif expr_type == '?':
            return If(gen_expr(), gen_expr(), gen_expr())
        else:
            raise TypeError(f'illegal expression type: {expr_type}')
    return gen_expr()

class TestCase(NamedTuple):
    expr: str
    environ: dict[str, int]
    result: str

    def __str__(self):
        buf: list[str] = ["(char*[]){"]
        for key, value in self.environ.items():
            buf.append(f'"{key}={value}", ')
        buf.append("NULL}")
        environ = ''.join(buf)
        return f'{{ "{self.expr}", {environ}, {self.result} }}'

# TODO: negative examples
def gen_testcase():
    while True:
        vars: set[str] = set()
        environ: dict[str, int] = {}
        expr = gen_expr(vars, 18)
        #print(expr)
        for var in vars:
            environ[var] = gen_value()
        str_expr = str(expr)
        try:
            expr.eval(environ)
        except ZeroDivisionError:
            continue
        except ValueError as exc:
            if exc.args[0] == 'shift out of range':
                continue
            raise
        result = expr.c_expr(environ)
        return TestCase(
            str_expr, environ, result
        )

if __name__ == '__main__':
    print('''\
#include "testdata.h"

#include <stddef.h>

// These are some randomly generated tests.

const struct TestCase TESTS[] = {''')
    for _ in range(1024):
        test = gen_testcase()
        print(f'    {test},')
    print('''\
    { NULL, NULL, 0 },
};''')
