include(`setup.m4')

\set_author([René Seindal])
\set_title([What is GNU m4])

\divert(1)

\p([GNU \tt(m4) is an implementation of the traditional Unix macro
processor.  GNU m4 is mostly compatible with the System V, Release 3
version, and SVR4, although it has some extensions (for example,
handling more than 9 positional parameters to macros). GNU \tt(m4)
also has built-in functions for including files, running shell
commands, doing arithmetic, etc.])

\p([GNU \tt(m4) is a macro processor, in the sense that it copies its
input to the output, expanding macros as it goes. Macros are either
builtin or user-defined, and can take any number of arguments. Besides
just doing macro expansion, m4 has builtin functions for including named
files, running UNIX commands, doing integer arithmetic, manipulating
text in various ways, recursion, etc... m4 can be used either as a
front-end to a compiler, or as a macro processor in its own right.])

\p([The m4 macro processor is widely available on all UNIXes. Usually,
only a small percentage of users are aware of its existence. However,
those who do often become commited users. The growing popularity of GNU
Autoconf, which prerequires GNU m4 for generating the `configure'
scripts, is an incentive for many to install it, while these people will
not themselves program in m4.])

\p([Some people found m4 to be fairly addictive. They first use m4 for
simple problems, then take bigger and bigger challenges, learning how to
write complex m4 sets of macros along the way. Once really addicted,
users pursue writing of sophisticated m4 applications even to solve
simple problems, devoting more time debugging their m4 scripts than
doing real work. Beware that m4 may be dangerous for the health of
compulsive programmers.])

\p([Autoconf needs GNU m4 for generating `configure' scripts, but not for
running them.])

\p([GNU m4 is a Unix program.  It is designed to work in a Unix-like
environment.  GNU m4 1.4 has, however, been ported to DJGPP, the GNU C
compiler for DOS/Windows.  These files are present in the
\link(download.htm, download area).])

\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
