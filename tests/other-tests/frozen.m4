divert(1)dnl
define(`test', `local::`test'')dnl
define(`test1', defn(`test'))dnl
->test
load(`modtest')
define(`test2', defn(`test'))dnl
->test
load(`shadow')
define(`test3', defn(`test'))dnl
->test
