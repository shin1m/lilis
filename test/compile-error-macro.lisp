(define-macro m (x)
  `(conz ,x '(world))
)
(print (m 'hello))
