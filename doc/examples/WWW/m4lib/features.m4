include(`setup.m4')

\set_author([René Seindal])
\set_title([New feaures since version 1.4])

\divert(1)

\p(Please look at the \link(news.htm, NEWS) and the \link(changelog.htm,
ChangeLog) for all the gory details.)

\dl(
\dt(\b(GNU m4 uses GNU Automake and GNU Autoconf for configuration.))

\dd(\p(This has been long overdue, and now hit is done thanks to Erick
Branderhorst.))

\dt(\b(GNU m4 uses GNU gettext for internationalisation.))

\dd(\p(GNU m4 now speaks several languages. Translations for
german, french, italian, japanese, dutch, polish, romenian and swedish
have been made.))

\dt(\b(Support for multiple precision arithmetic in eval.))

\dd(\p(If appropriately configured, GNU m4 can now do multiple precision
arithmetic in the built in macro 'eval'. If not configured, GNU m4
will use the largest integer available for its calculations.))

\dt(\b(An input syntax table to change how input is parsed.))

\dd(\p(A new build in macro 'changesyntax' allows finer control over how input
characters are parsed into input tokens.&nbsp; It is now possible to have
several one character quote strings or comment delimiters, to change the
format of macro calls, to use active characters like in TeX, and probably
most useful, to change what input characters are treated as letters when
looking for macro calls.)

\p(See the \link(man/m4_7.html#SEC41, manual section) for more details.))

\dt(\b(Support for loadable modules.))

\dd(\p(GNU m4 now has support for dynamic loading of compiled modules at
runtime. A module can define any number of new built in macros, which
will be indistinguishable from the standard set of built in
macros. Modules can also override existing built in macros.)
)

\dt(\b(Better control of sync-lines generation.))

\dd(\p(The new built in macro 'syncoutput' allows better control of the
generation of sync-lines. They can now be turned on or off at
will.))

)

\divert(0)\dnl
\DO_LAYOUT([\undivert(1)])
\divert(-1)
