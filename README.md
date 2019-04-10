# lilis [![Build Status](https://travis-ci.org/shin1m/lilis.svg?branch=master)](https://travis-ci.org/shin1m/lilis)

lilis is a LIttle LISp interpreter.
This is a kind of etude for myself to learn lisp concepts and how to implement them.

It has a minimal set of features:
* Symbols
* Pairs
* Quotes
* Quasi quates
* Lambda closures
* Static scoping
* Tail calls
* Macros
* Modules
* Delimited continuations
* Not implemented yet: integers, floats, strings


## Builtins

### (lambda ARGUMENTS EXPRESSIONS)

    ARGUMENTS: (SYMBOL*)
        | (SYMBOL+ . REST)
        | REST
    REST: SYMBOL
    EXPRESSIONS: EXPRESSION*

Instantiates a new lambda closure.

### (begin EXPRESSIONS)

Evaluates EXPRESSIONS in order.
The result of the last EXPRESSION is the result of this form.

### (define SYMBOL EXPRESSION)

Evaluates EXPRESSION and bind it to SYMBOL.
The bound value is the result of this form.

### (set! SYMBOL EXPRESSION)

Evaluates EXPRESSION and assign it to SYMBOL.
The assigned value is the result of this form.

### (define-macro SYMBOL ARGUMENTS EXPRESSIONS)

Defines a new macro and bind it to SYMBOL.

### (export SYMBOL)

Exports a binding to SYMBOL from the current module.

### (import SYMBOL)

Loads a module named SYMBOL.lisp and imports all exported symbols from the module into the current scope.

### (if CONDITION THEN [ELSE])

    CONDITION: EXPRESSION
    THEN: EXPRESSION
    ELSE: EXPRESSION

If CONDITION evaluates to other than `()`, evaluates THEN and the result of it is the result of this form.
Otherwise, evaluates ELSE and the result of it is the result of this form.

### (eq? EXPRESSION EXPRESSION)

Evaluates the left EXPRESSION and the right EXPRESSION in order.
If the both results are identical, a value other than `()` is the result of this form.
Otherwise, `()` is the result of this form.

### (pair? EXPRESSION)

Evaluates EXPRESSION.
If the result is a pair object, a value other than `()` is the result of this form.
Otherwise, `()` is the result of this form.

### (cons EXPRESSION EXPRESSION)

Evaluates the left EXPRESSION and the right EXPRESSION in order.
Instantiates a new pair object.

### (car EXPRESSION)

Evaluates EXPRESSION.
The car part of the result is the result of this form.

### (cdr EXPRESSION)

Evaluates EXPRESSION.
The cdr part of the result is the result of this form.

### (gensym)

Evaluates to a unique symbol object.

### (module)

Instantiates an anonymous module.

### (read [EOF])

    EOF: EXPRESSION

Reads an S-expression from the standard input stream.
When end of file is reached before an expression, evaluates to EOF if given or `()`.
Otherwise, an expression read from the stream is the result of this form.

### (eval EXPRESSION MODULE)

    MODULE: EXPRESSION

Evaluates EXPRESSION with MODULE.
MODULE is a module instantiated by (module).

### (print EXPRESSIONS)

Evaluates EXPRESSIONS in order and prints the results in one line.
`()` is the result of this form.

### (call-with-prompt TAG HANDLER THUNK)

    TAG: EXPRESSION
    HANDLER: (_ CONTINUATION EXPRESSIONS)
    THUNK: (_)

Sets up a new prompt tagged with TAG and calls THUNK.
When (abort-to-prompt TAG EXPRESSIONS) is evaluated during the execution of THUNK, it calls (HANDLER CONTINUATION EXPRESSIONS).
CONTINUATION is a continuation object.
(CONTINUATION EXPRESSION) resumes the execution of THUNK at the point where (abort-to-prompt TAG EXPRESSIONS) is evaluated passing EXPRESSION as the result.
If HANDLER is called, the result of it is the result of this form.
If the execution of THUNK reaches the end, the result of it is the result of this form.

### (abort-to-prompt TAG EXPRESSIONS)

Instantiates a new continuation object CONTINUATION delimited by the closest prompt tagged with TAG and calls (HANDLER CONTINUATION EXPRESSIONS).
EXPRESSION of (CONTINUATION EXPRESSION) is the result of this form.


## How to Build

    autoreconf -is
    ./configure
    make
    make install


## How to Run Tests

    make check


## License

The MIT License (MIT)

Copyright (c) 2018-2019 Shin-ichi MORITA

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
