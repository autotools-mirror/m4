#!/bin/sh

# gentest/len.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2795

. ${srcdir}/defs

cat <<\EOF >in
len()
len(`abcdef')
EOF

cat <<\EOF >ok
0
6
EOF

$M4 -d in >out

$CMP -s out ok

