include(`setup.m4')

\set_author([René Seindal])
\set_title([Development site])

\divert(1)
\h2([Current development version is \__m4_version__.])

\p([Development versions contain new features and experiments that might
or might not make it into the next official release. The current
development version contains among other things (browse the
\link([features.htm], [new features]) for more detail):])

\ul([
  \li Uses GNU Automake and GNU Autoconf for configuration.

  \li Uses GNU gettext for internationalisation.

  \li Support for multiple precision arithmetic in eval.

  \li An input syntax table to change how input is parsed.

  \li Support for loadable modules.

  \li Better control of sync-lines generation.

  \li Various bug-fixes.
])

\p([A new release is expected ready for Spring 2000.])

\p([GNU \tt(m4) 1.4 is from october 1994 and can be considered stable.])

\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
