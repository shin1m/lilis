(define hanoi (lambda (tower move)
  (define step (lambda (height towers from via to) (if height
    (step (cdr height)
      (move
        (step (cdr height) towers from to via)
        from to
      )
      via from to
    )
    (move towers from to)
  )))
  (step (cdr tower) (cons tower '(() ())) () '(()) '(() ()))
))
(define get (lambda (xs i) (if i
  (get (cdr xs) (cdr i))
  (car xs)
)))
(define put (lambda (xs i x) (if i
  (cons (car xs) (put (cdr xs) (cdr i) x))
  (cons x (cdr xs))
)))
(print (hanoi '(a b c d e) (lambda (towers from to)
  (define tower (get towers from))
  (print towers)
  (put
    (put towers from (cdr tower))
    to
    (cons (car tower) (get towers to))
  )
)))
