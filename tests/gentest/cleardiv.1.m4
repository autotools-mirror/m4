#!/bin/sh

# gentest/cleardiv.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2738

. ${srcdir}/defs

cat <<\EOF >in
divert(1)
Diversion one: divnum
divert(2)
Diversion two: divnum
divert(-1)
undivert
EOF

cat <<\EOF >ok
EOF

$M4 -d in >out

$CMP -s out ok

