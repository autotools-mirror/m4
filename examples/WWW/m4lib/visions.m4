include(`setup.m4')

\set_author([René Seindal])
\set_title([The Road Ahead])

\divert(1)
\h2([Possible features for future versions])


\ul(

\li \p(Guile can be used as an extension language so complicated macros can be
written in Scheme while still maintaining the m4 interface.)

\li\p(A kind of super-quotes can be added, quotes that aren't stripped
when read, as are normal quotes. These quotes should be stripped when
output. In that way text can be super-quote and consequently passed
untouched to the output.  It is a bit like comments, but there the
quotes are removed before output.)

)

\p(See also the \link(todo.htm, TODO) file.)


\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
