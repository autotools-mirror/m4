\divert(-1);
The semicolons are just to get GNU Emacs C mode to indent properly.

\define([C_TEXT], [text="#000000"]);
\define([C_LINK], [link="#0000EF"]);
\define([C_ALINK], [vlink="#51188E"]);
\define([C_VLINK], [alink="#FF0000"]);
\define([C_BG1], [bgcolor="#FFCC99"]);
\define([C_BG2], [bgcolor="#FF9900"]);
\define([C_BG3], [bgcolor="#CC6600"]);

\define([DO_HEADER],
	[\head([\title([GNU m4 - \defn([_TITLE])])],
	       [\meta_if_set([AUTHOR])],
	       [\meta_if_set([GENERATOR])],
	       [\meta_if_set([KEYWORDS])],
	       )]);

\define([DO_BODY],
	[\body([\C_TEXT \C_BG1 \C_LINK \C_VLINK \C_ALINK],
	       [\table([cellpadding=5 width="100%"],
		       [\tr([align=left valign=bottom],
			    [\td([align=center valign=center colspan="3" width="100%" \C_BG2],
				 [\h1([GNU m4])],
				 [\h2(\defn([_TITLE]))],
				 )],
			    )],
		       [\tr([],
			    [\td([align=left valign=top width="15%" \C_BG2],
				 [\include([menu.m4])],
				 )],
			    [\td([align=left valign=top width="90%"],
				 [$*],
				 )],
			    )],
		       )],
	       )]
    );

\define([DO_LAYOUT],
	[\doctype([HTML PUBLIC "-//W3C//DTD HTML 4.0 TRANSITIONAL//EN"])
\html([\DO_HEADER], [\DO_BODY([$*])])]
    );

\define([<], [&lt;]);
\define([>], [&gt;]);

\define([showlink], [\link($1, $1)]);
\define([mailto], [\link(mailto:$1, $1)]);

