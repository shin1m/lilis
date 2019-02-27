(define-macro m (x) (cons 'print (cons ''hello (cons x ()))))
(m 'world)
(define-macro m (x) `(print 'hello ,x))
(m 'world)
(import assert)
(define-macro quote-list (x . y) `(cons ',x '(little ,@y)))
(print-assert-equal (quote-list hello lisp world) '(hello little lisp world))
(define-macro quote-list x `',x)
(print-assert-equal (quote-list hello world) '(hello world))
(print-assert-equal (quote-list hello world . ()) '(hello world))
(print-assert-equal (quote-list hello . (world)) '(hello world))
(print-assert-equal (quote-list . (hello world)) '(hello world))
