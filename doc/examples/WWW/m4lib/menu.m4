\pushdef([header], [\tr([\td([\C_BG3], [\p([\b([$1])])])])])

\pushdef([separator], [\tr([\td([height=5], [])])])

\pushdef([_row], [\link([$1], [$2])])

\pushdef([_rows],
  [\ifelse($#, 0, [],
    $#, 1, [],
    $#, 2, [\_row([$1], [$2])],
    $#, 3, [\_row([$1], [$2])],
    [\_row([$1], [$2])\br\n\_rows(\shift(\shift($@)))])])

\pushdef([rows], [\tr([\td([\p([\font([size=-1], [\b([\_rows($@)])])])])])])

\table([],
  [\header([General info])],
  [\rows(
    [whatis.htm], [What is m4],
    [features.htm], [Features],
    [uses.htm], [Uses of m4],
  )],
  [\separator],

  [\header([Documentation])],
  [\rows(
    [man/m4_toc.html], [Manual],
  )],
  [\separator],

  [\header([Source files])],
  [\rows(
    [readme.htm], [README],
    [todo.htm], [TODO],
    [news.htm], [NEWS],
    [changelog.htm], [ChangeLog],
    [thanks.htm], [Contributors],
    [m4/], [Browse it],
  )],
  [\separator],

  [\header([The Future])],
  [\rows(
    [modules.htm], [Modules],
    [visions.htm], [Visions],
  )],
  [\separator],

  [\header([Feedback])],
  [\rows(
    [lists.htm], [Mailing-lists],
    [feedback.htm], [Feedback],
    [/forum/list.php3?num=2], [Discussion Forum],
  )],
  [\separator],

  [\header([Development])],
  [\rows(
    [download.htm], [Download],
    [bugs.htm], [Known bugs],
  )],
  [\separator],

  [\header([Examples])],
  [\rows(
    [thissite.htm], [This site],
  )],
)

\popdef([header])
\popdef([rows])
\popdef([_rows])
\popdef([_row])
\popdef([separator])
