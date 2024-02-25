minmath
=======

An example parser for a tiny language that can perform addition, subtraction,
multiplication, division, and negation on integers while adhering to operator
precedence.

This demonstrates how to write a simple recursive descent parser for a simple
grammar and demonstrates how it automatically adheres to operator precedence
if you just order your rules correctly (i.e. from weakest to strongest
precedence).

Syntax
------

### Operator Precedence

1. atoms, parenthesized expressions
2. negation (unary `-`)
3. multiplication, division
4. addition, subtraction

Operations of the same precedence are performed left to right.

### Tokens

- `+`
- `-`
- `*`
- `/`
- `(`
- `)`
- integers
- variable identifiers

All white-space is skipped. Single line comments starting with `#` are supported.

### Backus–Naur Form (BNF)

In order to ensure operator precedence simply start with the weakest operation
and recurse to the strongest (i.e. atoms or parenthesized expressions).
Operations that have the same precedence (like e.g. addition and subtraction)
are handled in the same rule as alternatives in a list of these operations.

This is really all you need, no more head scratching than that.

```BNF
EXPRESSION := SUM
SUM        := PRODUCT (("+" | "-") PRODUCT)*
PRODUCT    := UNARY (("*" | "/") UNARY)*
UNARY      := "-"* ATOM
ATOM       := VARIABLE | INTEGER | "(" EXPRESSION ")"

VARIABLE   := ("a" ... "z" | "a" ... "z" | "_") ("a" ... "z" | "a" ... "z" | "_" | "0" ... "9")*
INTEGER    := ("+" | "-")? ("0" ... "9")+

COMMENT    := "#" NOT_NEWLINE* ("\n" | EOF)
```

Writing a Parser
----------------

To write a parser for this language first write a tokenizer. A tokenizer takes
the source of the language as input and produces a stream of tokens while
skipping any ignorable characters, like white-space and comments. The tokenizer
has the ability to look ahead one single token without consuming it (peeking).
The actual parser operates on these tokens.

To write the parser simply write one function for each rule in the BNF that
aren't corresponding to tokens (and thus already handled by the tokenizer).
Where another rule is mentioned you do a recursive call to the corresponding
parser function. `(` `)*` is translated to loops. The loop condition needs to
look ahead one single token to be able to determine when to break. Most
programming languages don't need more lookahead than one single token.

That's it. Enjoy!

There are other more complicated ways to write much more efficient parsers,
but as a first step this good enough.
