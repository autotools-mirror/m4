# no modules loaded yet
test
shadow

# define our own macros for `test' and `shadow'
define(`test', `local::`test'')
define(`shadow', `local::`shadow'')
test
shadow

# save our local `shadow' macro until the Shadow module is unloaded
pushdef(`shadow')

# module Shadow defines `shadow' and `test' macros
loadmodule(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# save the definition of `test' from the Shadow module
define(`Shadow::test', defn(`test'))

# module Test also defines a `test' macro
loadmodule(`test') 
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Reloading Shadow shouldn't affect anything
loadmodule(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Test will not unshadow the test definition in Shadow without
# some macro magic
unloadmodule(`test')
define(`test', defn(`Shadow::test'))
undefine(`Shadow::test')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Shadow once has no effect (we loaded it twice)
unloadmodule(`shadow')
dumpdef(`test')
dumpdef(`shadow')
test
shadow

# Unloading Shadow again will revert to copying `test' and the locally
# pushed `shadow' macro.
unloadmodule(`shadow')
test
shadow
