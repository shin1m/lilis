(define f (lambda (return) (return 'x) 'y))
(print (f (lambda (x) x)))
(print (call/cc f))
