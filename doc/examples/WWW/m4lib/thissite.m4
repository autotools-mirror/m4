include(`setup.m4')

\set_author([René Seindal])
\set_title([This site])

\divert(1)

\p([This GNU m4 site is maintained by René Seindal,
(\mailto(rene@seindal.dk)).])

\p([All files are generated using GNU m4 \__m4_version__.  You can view
the \link(m4lib/, source files).  They are very simple.  They use some
features from GNU m4 1.4l])

\p([The basic M4 definitions of quotes, comments, escapes are in
\showlink(m4lib/setup.m4).  This is first included by all files to
configure the enviroment correctly for the other files.  To avoid have
macros called by accident, an escape character is defined with
changesyntax.  \i(This is a new feature in m4 1.4l).])

\p([Some fairly general macros to generate various HTML construct are
found in \showlink(m4lib/html.m4).  There are macros for simple tags,
containers with and without attributes, links and a few utility macros.])

\p([The visual aspects of the pages are in \showlink(m4lib/layout.m4).
The macros herein generate the complete HTML structure for the pages.
There are macros for making the header and the body of the document.])

\p([The definition of the left hand menu is in \showlink(m4lib/menu.m4).
I convinced GNU Emacs to do the indentation by switching to c-mode.])

\p([The page body is passed to the layout definitions as an argument.  As
the text can be large, it is first diverted and the text passed to the
layout macros is simply a call to undivert.  That way a very large text
can be passed around with very little cost.  This page is made with
\link(m4lib/thissite.m4, these definitions).])

\p([There is a single file for each HTML file.])

\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
