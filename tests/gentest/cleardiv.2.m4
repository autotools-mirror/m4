#!/bin/sh

# gentest/cleardiv.2.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2753

. ${srcdir}/defs

cat <<\EOF >in
define(`cleardivert',
`pushdef(`_num', divnum)divert(-1)undivert($@)divert(_num)popdef(`_num')')
EOF

cat <<\EOF >ok

EOF

$M4 -d in >out

$CMP -s out ok

