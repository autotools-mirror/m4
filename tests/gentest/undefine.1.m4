#!/bin/sh

# gentest/undefine.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1186

. ${srcdir}/defs

cat <<\EOF >in
foo
define(`foo', `expansion text')
foo
undefine(`foo')
foo
EOF

cat <<\EOF >ok
foo

expansion text

foo
EOF

$M4 -d in >out

$CMP -s out ok

