TEST_EXTENSIONS = .lisp .repl
LISP_LOG_COMPILER = $(top_builddir)/lilis
AM_LISP_LOG_FLAGS = --debug --verbose
REPL_LOG_COMPILER = $(srcdir)/run-repl $(top_builddir)/lilis
scripts = \
	hello.lisp \
	macro.lisp \
	condition.lisp \
	set.lisp \
	import.lisp \
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
	repl-test.repl
TESTS = $(scripts)
EXTRA_DIST = $(scripts) \
	assert.lisp \
	boolean.lisp \
	peano.lisp \
	shiftreset.lisp \
	callcc.lisp \
	run-repl \
	repl-test.reple
