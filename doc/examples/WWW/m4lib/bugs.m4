include(`setup.m4')

\set_author([René Seindal])
\set_title([Known bugs in GNU m4 \__m4_version__])

\divert(1)
\h2([Known bugs in GNU m4])

\define([fixed], [\p([Fixed in version 1.4$1])])

\define([notme], [\p([A <A
HREF="mailto:m4-feedback@seindal.dk?subject=GNU m4: \defn([_item])"
>volunteer</A> is badly needed for this, as I have no way of testing
this myself.])])

\ul([

\item([undivert], [undivert(0) might read from standard output],

[\p([If calling \tt(undivert(0)) when diverting to a non-zero diversion
will cause m4 to read from standard output in an attempt to bring back
diversion 0, which is not possible.])

\fixed(n)

])

\item([sigaltstack], [failure if sigaltstack or sigstack returns ENOSYS],

[\p([If stack overflow detection is configured but the system doesn't
support sigaltstack(2) or sigstack(2), m4 fails when the system call
returns ENOSYS.  It should silently revert to default behaviour.])

\notme
])

])

\p([See also the \link(todo.htm, TODO) file.])

\print_items

\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)


\item([], [],

[\p([])

])
\undivert
