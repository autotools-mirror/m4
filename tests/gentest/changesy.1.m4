#!/bin/sh

# gentest/changesy.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2125

. ${srcdir}/defs

cat <<\EOF >in
define(`test.1', `TEST ONE')
__file__
changesyntax(`O_', `W.')
__file__
test.1
EOF

cat <<\EOF >ok

in

__file__
TEST ONE
EOF

$M4 -d in >out

$CMP -s out ok

