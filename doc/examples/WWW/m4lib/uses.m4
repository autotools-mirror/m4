include(`setup.m4')

\set_author([René Seindal])
\set_title([Current uses of m4])

\divert(1)

\p(The MTA sendmail uses \tt(m4) for generating configuration files.)

\p(\link(http://www.gnu.org/software/autoconf/autoconf.html, GNU
Autoconf) uses \tt(m4) to generate "configure" scripts, that are used
for configuring \link(http://www.gnu.org/, GNU) software for a
particular platform.)

\p(Htm4l is a set of macros for generating HTML.  Html4 is written by
Terry Jones (terry@cliffs.ucsd.edu).  See
\showlink(http://cliffs.ucsd.edu/terry/htm4l/htm4l/main.html) for
details. )

\p(Various programs uses m4 to preprocess configuration files, for
example the X11 window manager fvwm.)

\p(There is an \link(http://www.ssc.com/lg/issue22/using_m4.html,
article in the Linux Gazette) about writing HTML with GNU m4 written by
\link(mailto:bhepple@bit.net.au, Bob Hepple) . More recent versions
are kept at \link(http://www.bit.net.au/~bhepple, Bob's home site).
The macros are used to maintain a large commercial site at
\showlink(http://www.finder.com.au).)

\p(Other examples of GNU m4 generated HTML pages, written by
\link(mailto:max@alcyone.com, Erik Max Francis) can be found at the sites
\showlink(http://www.alcyone.com/max/),
\showlink(http://www.catcam.com/),
\showlink(http://www.crank.net/) and
\showlink(http://www.pollywannacracka.com/).
)

\p(\link(thissite.htm, These files are created with GNU m4 \__m4_version__).)


\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
