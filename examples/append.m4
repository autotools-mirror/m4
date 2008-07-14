dnl Stress test for appending algorithm.  Usage:
dnl m4 -Ipath/to/examples [-Doptions] append.m4
dnl Options include:
dnl -Dlimit=<num> - set upper limit of sequence to <num>, default 1000
dnl -Dverbose - print progress to the screen, rather than discarding
dnl -Dnext=<code> - append <code> each iteration, default to the current count
dnl -Ddebug[=<code>] - execute <code> after loop
dnl -Dsleep=<num> - sleep for <num> seconds before exit, to allow time
dnl   to examine peak process memory usage
include(`forloop2.m4')dnl
ifdef(`limit', `', `define(`limit', `1000')')dnl
ifdef(`verbose', `', `divert(`-1')')dnl
ifdef(`next', `', `define(`next', `i')')dnl
ifdef(`debug', `', `define(`debug')')dnl
define(`var')define(`append', `define(`var', defn(`var')`$1')')dnl
forloop(`i', `1', limit, `i
append(next)')debug
ifdef(`sleep',`syscmd(`echo done>/dev/tty;sleep 'sleep)')dnl
