#!/bin/sh

# gentest/define.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 987

. ${srcdir}/defs

cat <<\EOF >in
define(`foo', `Hello world.')
foo
EOF

cat <<\EOF >ok

Hello world.
EOF

$M4 -d in >out

$CMP -s out ok

