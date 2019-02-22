(define-macro m (x) (cons 'print (cons ''hello (cons x ()))))
(m 'world)
(define-macro m (x) `(print 'hello ,x))
(m 'world)
