include(`setup.m4')

\divert(1)

\define([_ideas], [])

\define([register_idea],
[\define([H_$1], [$2])\dnl
\define([T_$1], [$3])\dnl
\define([_ideas], [\print_idea([$1])]\defn([_ideas]))])

\define([print_idea], [
\target([$1], [\h2([\indir([H_$1])])])
\indir([T_$1])
])

\define([print_ideas], [\indir([_ideas])])

\define([idea], [\li \p([\link([[#]$1], [$2.])])\register_idea([$1], [$2], [$3])])

\idea([guile], [Guile as an extension language], [gfhjdsfsarhgew])
\idea([pquote], [Persistent quotes],[asdffhfdghgdsfh])
\idea([deps], [Dependencies generation],[afsdffasdf])

\print_ideas

\undivert(1)

\defn([_ideas])
