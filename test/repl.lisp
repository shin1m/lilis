(define loop (lambda (module eof)
  (define x (read eof))
  (if (eq? x eof) () (begin
    (print (eval x module))
    (loop module eof)
  ))
))
(define top (module))
(eval '(begin
  (define-macro -define-macro () define-macro)
  (define-macro define-macro (name arguments . body) `(begin
    (,(-define-macro) ,name ,arguments . ,body)
    (export ,name)
    ,name
  ))
  (export define-macro)
  (define-macro -define () define)
  (define-macro define (name value) `(begin
    (,(-define) ,name ,value)
    (export ,name)
  ))
  (export define)
  (define-macro -set! () set!)
  (define-macro set! (name value) `(begin
    (,(-set!) ,name ,value)
    (export ,name)
  ))
  (export set!)
) top)
(loop top (gensym))
