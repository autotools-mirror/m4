\pushdef([header], [\tr([\td([\C_BG3], [\p([\b([$1])])])])])
\pushdef([row], [\tr([\td([\font([size=-1], [\p([\b([\link([$1], [$2])])])])])])])

\table([align=left valign=top columns=1],
       [\header([Generel info])],
       [\row([whatis.htm], [What is m4])],
       [\row([features.htm], [Features])],
       [\row([uses.htm], [Uses of m4])],

       [\header([Documentation])],
       [\row([man/m4_toc.html], [Manual])],

       [\header([Source files])],
       [\row([readme.htm], [README])],
       [\row([todo.htm], [TODO])],
       [\row([news.htm], [NEWS])],
       [\row([changelog.htm], [ChangeLog])],
       [\row([thanks.htm], [Contributors])],
       [\row([m4/], [Browse it])],

       [\header([The Future])],
       [\row([modules.htm], [Modules])],
       [\row([visions.htm], [Visions])],

       [\header([Development])],
       [\row([lists.htm], [Mailing-lists])],
       [\row([feedback.htm], [Feedback])],
       [\row([download.htm], [Download])],

       [\header([Examples])],
       [\row([thissite.htm], [This site])],
       )

\popdef([header])
\popdef([row])
