dnl Copyright (C) 2006, 2010, 2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it
dnl with or without modifications, as long as this notice is preserved.
# no modules loaded yet
test
shadow

# define our own macros for `test' and `shadow'
define(`test', `local::`test'')
define(`shadow', `local::`shadow'')
test
shadow

# module Shadow defines `shadow' and `test' macros
load(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# save the definition of `test' from the Shadow module
define(`Shadow::test', defn(`test'))

# module Test also defines a `test' macro
load(`modtest')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Reloading Shadow shouldn't affect anything
load(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Test will unshadow the test definition in Shadow
unload(`modtest')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Shadow once has no effect (we loaded it twice)
unload(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Shadow again will revert to copying `test' and the local
# `shadow' macro.
unload(`shadow')
test
shadow
