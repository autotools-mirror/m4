#!/bin/sh

# gentest/changesy.4.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2177

. ${srcdir}/defs

cat <<\EOF >in
define(`@', `TEST')
@
changesyntax(`A@')
@
EOF

cat <<\EOF >ok

@

TEST
EOF

$M4 -d in >out

$CMP -s out ok

