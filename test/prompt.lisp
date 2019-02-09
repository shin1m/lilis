(import assert)
(define c (call-with-prompt 'foo
  (lambda (k) k)
  (lambda ()
    (cons 'hello (cons (abort-to-prompt 'foo) ()))
  )
))
(print-assert-equal (c 'world) '(hello world))
