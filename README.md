# lilis [![Build Status](https://travis-ci.org/shin1m/lilis.svg?branch=master)](https://travis-ci.org/shin1m/lilis)

lilis is a LIttle LISp interpreter.
This is a kind of etude for myself to learn lisp concepts and how to implement them.

It has a minimal set of features:
* Symbols
* Pairs
* Quotes
* Quasi quotes
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

### (eq? x y)

    x: OBJECT
    y: OBJECT

If `x` and `y` are identical, returns a value other than `()`.
Otherwise, returns `()`.

### (pair? x)

    x: OBJECT

If `x` is a PAIR object, returns a value other than `()`.
Otherwise, returns `()`.

### (cons head tail)

    head: OBJECT
    tail: OBJECT

Instantiates a new PAIR object with car part `head` and cdr part `tail`.

### (car x)

    x: PAIR

Returns the car part of `x`.

### (cdr x)

    x: PAIR

Returns the cdr part of `x`.

### (gensym)

Instantiates a new unique SYMBOL object.

### (module)

Instantiates a new anonymous MODULE object.

### (read [eof])

    eof: OBJECT

Reads an S-expression from the standard input stream.
When end of file is reached before an expression, returns `eof` if given or `()`.
Otherwise, returns an expression read from the stream.

### (eval expression module)

    expression: OBJECT
    module: MODULE

Evaluates `expression` with `module`.
`module` is a MODULE object instantiated by `(module)`.
Returns the result of evaluation.

### (print values...)

    values: list of OBJECT

Prints `values` in one line.
Returns `()`.

### (call-with-prompt tag handler thunk)

    tag: OBJECT
    handler: (_ continuation values...)
        continuation: CONTINUATION
        values: list of OBJECT
    thunk: (_)

Sets up a new prompt tagged with `tag` and calls `thunk`.
When `(abort-to-prompt tag values...)` is evaluated during the execution of `thunk`, it calls `(handler continuation values...)`.
`continuation` is a CONTINUATION object.
`(continuation value)` resumes the execution of `thunk` at the point where `(abort-to-prompt tag values...)` is evaluated passing `value` as the result.
If `handler` reaches the end, returns the result of it.
If the execution of `thunk` reaches the end, returns the result of it.

### (abort-to-prompt tag values...)

    tag: OBJECT
    values: list of OBJECT

Instantiates a new CONTINUATION object `continuation` delimited by the closest prompt tagged with `tag` and calls `(handler continuation values)`.
Returns `value` of `(continuation value)`.

### (error message)

    message: OBJECT

Instantiates a new ERROR object with a message `message`.

### (catch continuation error)

    continuation: CONTINUATION
    error: ERROR

Extracts call stack of `continuation` and append them to `error` as backtrace.
`catch` is also used as the tag for system errors.
So system errors can be handled by `(call-with-prompt catch ...)`.
Returns `error`.

## How to Build and Run Tests

    mkdir build
    cd build
    cmake ..
    cmake --build.
    ctest

## License

The MIT License (MIT)

Copyright (c) 2018-2021 Shin-ichi MORITA

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
