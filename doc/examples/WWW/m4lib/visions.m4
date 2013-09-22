include(`setup.m4')

\set_author([René Seindal])
\set_title([The Road Ahead])

\define([originator],
[\p([Idea contributed by [$1]]\ifelse($#, 2, [ (\mailto([$2]))])[.])])

\define([noone], [\p([There is no-one working on this now.  Do you want
to <A HREF="mailto:m4-feedback@seindal.dk?subject=GNU m4: \defn([_item])" >volunteer</A>?])])


\define([done], [\p([Done in version 1.4$1])])


\divert(1)
\h2([Ideas for future versions of GNU m4])

\p([Here are some ideas and suggestion for the future of GNU m4, large
and small.  The order here is fairly random.])

\ul([

\item([guile], [Guile as an extension language],

[\p([\link([http://www.red-bean.com/guile/], [Guile]) can be used as an
extension language so complicated macros can be written in Scheme while
still maintaining the m4 interface.  It will require some changes to the
base code, as guile cannot be used from a module.])

\noone <!-- \originator([René Seindal], [rene@seindal]) -->
])


\item([utf8], [UTF-8 or wide characters],

[\p([GNU m4 should be able to handle UTF-8 input or wide characters so
it can be more usable for different environments.])

\noone <!-- \originator([François Pinard]) -->
])


\item([pquote], [Syntax: persistent quotes],

[\p([Persistent quotes is a way of getting text unharmed through m4's
processing.  While normal quotes are stripped when a quoted string is
read, the persistent quotes are removed just before being output.  This
will ensure that the quoted text is always output verbatim.])

\p([The bulk of the changes will be in the parser (in input.c function
next_token).  Persistent quotes cannot be nested, they must balance
within a normally quoted string, but normal quotes need not balance
within persistent quotes (neither within persistent quotes within normal
quotes).  The quotes should be removed before being shipped out (in
macro.c).])

\noone <!-- \originator([Keith Bostic]) -->
])




\item([comment2], [Syntax: removable comments],

[\p([With the syntax table a category for discardable comments can be
defined, causing that type of comments to be discarded.])

\noone
])




\item([comment1], [Option:  remove comments],

[\p([There should be an option (--discard-comments) to get m4 to discard
comments instead of passing them to the output.])

\done(n)
])



\item([deps], [Option: show dependencies],

[\p([There should be an options to generate makefile dependencies for an
M4 input file.])

\p([It is not enough to scan the files for includes, as file names can
be generated and builtins renamed.  To make it work, m4 will have to do
a complete run of the input file, discard the output and print the
dependencies instead.])

\p([It cannot be made to work in all cases when input file names are
generated on the fly.])

\noone  <!-- \originator([Erick Branderhorst]) -->
])


\item([safer], [Option: render GNU m4 safer],

[\p([There should be a --safer option that disables all functions, that
could compromise system security if used by root.  It will have to
include various functions, such as file inclusion, sub shells, module
loading, ...])

\noone  <!-- \originator([Santiago Vila]) -->
])



\item([import], [Option: import environment],

[\p([An option to defined each environment variable as a macro on
startup would be useful in many cases.])

\done(n)  <!-- \originator([René Seindal]) -->
])



\item([m4expand], [Builtin: quote expanded text],

[\p([A builtin to quote expanded text would be useful.  Now it is not
possible to quote the expansion of a macro; the macro itself has to
provide the quotes.  Some builtins return quoted strings, others
don't.])

\p([A possible solution is a build in macro that takes one argument.  It
expands this argument fully and returns the quoted expansion.])

\p([It will require changes to input handling and macro expansion code.])

\noone  <!-- \originator([Axel Boldt]) -->
])



\item([perl], [Module: embedded perl],

[\p([Perl could be embedded in m4, giving users a powerful programming
language for writing macros.  A single builtin "perleval" could do the
job.  First argument could be a perl function and the rest arguments.
The return value of the function as a string would be the expansion.])

\p([The perl interpreter should be set up when the module is loaded and
closed down before m4 exits, using the appropriate hooks in the module
interface.])

\p([A perl module could potentially give users access to any facility
perl has access to, such as databases.])

\p([On systems with perl compiled as a shared library the size penalty
would be minimal.])

\p([(It might not be workable as a module, as it will need to link with non-shared libraries.  Don't know how it can be fixed.  (RS))])

\noone <!-- \originator([René Seindal]) -->
])



\item([output], [Module: better output control],

[\p([It has been suggested a couple of times that it should be possible
to divert to named files, in order to create several output files.])

\p([I think this a bit a misunderstanding.  Diversion are inteded to be
brought back later, ie, they are temporary and recoverable.  Output
text, on the other hand, once output it is lost (for m4).  Therefore
better output control should be made in a different way.])

\p([My suggestion is a set of builtins defined by a module:])

\pre([setoutput(file)
appendoutput(file)
pipeoutput(command)])

\p([With these output can be directed better, diversion can be sent to
different files, and groups of files can be built by a single m4 run.
Calling \tt(setoutput) without arguments should resume output to
standard output.])

\p([(Admittedly, diversion 0 (standard output) has always been
different, as it cannot be undiverted.)])

\noone <!-- \originator([René Seindal]) -->
])



\item([require], [Module: require/provide functionality],

[\p([Two new builtins \tt(require) and \tt(provide) could provide a
handy interface to include.  It has proven difficult to write these
robustly as normal macros.  As an example, the files \tt(test.m4) and
\tt(../test.m4) could be the same file or different files depending on
the search path.])

\noone <!-- \originator([Terry Jones]) -->
])


])



\p([See also the \link(todo.htm, TODO) file.])

\print_items



\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)

\divert(3)saljdfnaskdjfndsa\divert(-1)




\item([], [],

[\p([])

\noone
])

\undivert
