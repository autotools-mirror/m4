\define([n], [
])

\define([concat], [\ifelse($#, 0, ,
  $#, 1, [$1],
  [$1 \concat(\shift($@))])])

\define([toupper], [\translit([$*], [a-z], [A-Z])])

\define([container],
[\pushdef([_tag], \toupper([$1]))\dnl
\ifelse($#, 1, [<\_tag></[\_tag]>],
  $#, 2, [<\_tag>$2</\_tag>],
  $#, 3, [<\_tag $2>$3</\_tag>],
    [<\_tag $2>\concat(\shift(\shift($@)))</\_tag>])\dnl
\popdef([_tag])\dnl
])

\define([large_container],
[\pushdef([_tag], \toupper([$1]))\dnl
\ifelse($#, 1, [<\_tag></\_tag>\n],
  $#, 2, [<\_tag>\n[]$2\n</\_tag>\n],
  $#, 3, [<\_tag $2>\n[]$3\n</\_tag>\n],
    [<\_tag $2>\n\concat(\shift(\shift($@)))\n</\_tag>\n])\dnl
\popdef([_tag])\dnl
])

\define([large_simple_container],
[\pushdef([_tag], \toupper([$1]))\dnl
<\_tag>\n\concat(\shift($@))\n</\_tag>\n\dnl
\popdef([_tag])\dnl
])

\define([simple_container],
[\pushdef([_tag], \toupper([$1]))\dnl
<\_tag>\concat(\shift($@))</\_tag>\dnl
\popdef([_tag])\dnl
])

\define([simple_tag],
[\pushdef([_tag], \toupper([$1]))\dnl
\ifelse([$2], [], [<\_tag>], [<\_tag $2>])\dnl
\popdef([_tag])\dnl
])

\define([doctype], [\simple_tag([!DOCTYPE], $@)])

\define([html], [\large_simple_container([$0], $@)])
\define([head], [\large_simple_container([$0], $@)])
\define([title], [\simple_container([$0], $@)])

\define([meta], [\n<META NAME="[$1]" CONTENT="[$2]">])
\define([http_equiv], [\n<META HTTP-EQUIV="[$1]" CONTENT="[$2]">])

\define([body], [\large_container([$0], $@)])

\define([center], [\large_simple_container([$0], $@)])
\define([right], [\large_simple_container([$0], $@)])
\define([left], [\large_simple_container([$0], $@)])
\define([div], [\large_container([$0], $@)])

\define([b], [\simple_container([$0], $@)])
\define([i], [\simple_container([$0], $@)])
\define([tt], [\simple_container([$0], $@)])

\define([table], [\large_container([$0], $@)])
\define([tr], [\large_container([$0], $@)])
\define([td], [\large_container([$0], $@)])
\define([th], [\large_container([$0], $@)])

\define([link], [<A HREF="$1">\shift($*)</A>])
\define([target], [<A NAME="$1">\shift($*)</A>])

\define([font], [\n\container([$0], $@)\n])

\define([h1], [\n\container([$0], $@)\n])
\define([h2], [\n\container([$0], $@)\n])
\define([h3], [\n\container([$0], $@)\n])
\define([h4], [\n\container([$0], $@)\n])
\define([h5], [\n\container([$0], $@)\n])
\define([h6], [\n\container([$0], $@)\n])

\define([p], [\large_simple_container([$0], $@)])

\define([hr], [\simple_tag([$0], $@)])

\define([ul], [\large_container([$0], $@)])
\define([ol], [\large_container([$0], $@)])

\define([li], [\simple_tag([$0], $@)])

\define([blockquote], [\large_simple_container([$0], $@)])

\define([dl], [\large_simple_container([$0], $@)])
\define([dt], [\simple_container([$0], $@)])
\define([dd], [\large_simple_container([$0], $@)])

\define([br], [\simple_tag([$0], $@)])
\define([hline], [\simple_tag([$0], $@)])

\define([pre], [\simple_container([$0], $@)])



\define([set_title], [\define([_TITLE], [$*])])
\set_title(_TITLE)

\define([set_author], [\define([_AUTHOR], [$*])])
\set_author()

\define([set_generator], [\define([_GENERATOR], [$*])])
\set_generator([GNU m4 \__m4_version__])

\define([set_keywords], [\define([_KEYWORDS], [$*])])
\set_keywords()

\define([set_body], [\define([_BODY], [$*])])
\set_body()

\define([meta_if_set],
  [\ifelse(\defn([_$1]), [], [], \meta([$1], \defn([_$1])))]\dnl
)
