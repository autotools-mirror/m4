#!/bin/sh

# gentest/argument.2.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1046

. ${srcdir}/defs

cat <<\EOF >in
define(`exch', `$2, $1')
define(exch(``expansion text'', ``macro''))
macro
EOF

cat <<\EOF >ok


expansion text
EOF

$M4 -d in >out

$CMP -s out ok

