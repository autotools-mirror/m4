divert(`-1')
# curry(macro, args...)
# Perform partial argument application on the given macro.  This
# expands to an unspecified macro name that accepts one or more extra
# arguments, and appends those to the args supplied to the original
# curry call as the overall set of arguments to pass to macro.  That
# is, curry(`macro', args1...)(args2...) is the same as invoking
# macro(args1..., args2...).
#
# Most often, argument currying comes in handy when given a context
# that normally takes a macro name to call with one argument, but
# where you want to combine that variable argument with other fixed
# arguments to forward to a macro that takes multiple arguments.  For
# example, given a "foreach" macro that calls its first argument once
# for each successive argument, "foreach(`curry(`mult', 3)', 1, 2, 3)"
# would behave the same as "mult(3, 1), mult(3, 2), mult(3, 3)".
#
# It is also possible to create a named function curry.  For example:
# define(`mult3', `curry(`mult', 3)($1)')
# Later use of mult3(value) will compute the same as mult(3, value).
define(`curry', `$1(shift($@,)_curry')
define(`_curry', `$@)')
divert`'dnl
