#!/bin/sh

# gentest/translit.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2932

. ${srcdir}/defs

cat <<\EOF >in
translit(`GNUs not Unix', `A-Z')
translit(`GNUs not Unix', `a-z', `A-Z')
translit(`GNUs not Unix', `A-Z', `z-a')
EOF

cat <<\EOF >ok
s not nix
GNUS NOT UNIX
tmfs not fnix
EOF

$M4 -d in >out

$CMP -s out ok

