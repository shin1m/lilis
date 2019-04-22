(import assert)
(define m (module))
(print-assert-equal (eval '(cons 'hello '(world)) m) '(hello world))
