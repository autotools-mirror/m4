#!/bin/sh

# gentest/changesy.7.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2234

. ${srcdir}/defs

cat <<\EOF >in
define(`test', `==$1==')
changequote(`<<', `>>')
changesyntax(<<L[>>, <<R]>>)
test(<<testing]>>)
test([testing>>])
test([<<testing>>])
EOF

cat <<\EOF >ok



==testing]==
==testing>>==
==<<testing>>==
EOF

$M4 -d in >out

$CMP -s out ok

