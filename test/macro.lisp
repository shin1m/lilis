(define-macro m (x) (cons 'print (cons ''hello (cons x ()))))
(m 'world)
