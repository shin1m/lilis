(define c (call-with-prompt 'foo
  (lambda (k) k)
  (lambda ()
    (cons 'hello (cons (abort-to-prompt 'foo) ()))
  )
))
(print (c 'world))
