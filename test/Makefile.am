TEST_EXTENSIONS = .lisp .lispo .repl
LISP_LOG_COMPILER = $(top_builddir)/src/lilis
AM_LISP_LOG_FLAGS = --debug --verbose
LISPO_LOG_COMPILER = $(srcdir)/run-lispo $(top_builddir)/src/lilis
REPL_LOG_COMPILER = $(srcdir)/run-repl $(top_builddir)/src/lilis $(srcdir)/repl.lisp
scripts = \
	hello.lisp \
	condition.lisp \
	set.lisp \
	begin.lisp \
	rest.lisp \
	import.lisp \
	quasiquote.lisp \
	macro.lisp \
	macro-rest.lisp \
	macro-test.lisp \
	gensym.lisp \
	eval.lisp \
	boolean-test.lisp \
	peano-test.lisp \
	fibonacci.lisp \
	hanoi.lisp \
	prompt.lisp \
	shiftreset-test.lisp \
	shiftreset-yield.lisp \
	shiftreset-tail.lisp \
	callcc-test.lisp \
	callcc-generate.lisp \
	catch.lispo \
	compile-error-cast.lispo \
	compile-error-nil.lispo \
	compile-error-symbol.lispo \
	compile-error-macro.lispo \
	compile-error-export.lispo \
	compile-error-import.lispo \
	runtime-error.lispo \
	repl-test.repl
TESTS = $(scripts)
EXTRA_DIST = $(scripts) \
	assert.lisp \
	boolean.lisp \
	peano.lisp \
	shiftreset.lisp \
	callcc.lisp \
	run-lispo \
	catch.lispoe \
	compile-error-cast.lispoe \
	compile-error-nil.lispoe \
	compile-error-symbol.lispoe \
	compile-error-macro.lispoe \
	compile-error-export.lispoe \
	bad-macro.lisp \
	compile-error-import.lispoe \
	runtime-error.lispoe \
	repl.lisp \
	run-repl \
	repl-test.reple
