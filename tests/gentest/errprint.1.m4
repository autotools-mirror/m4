#!/bin/sh

# gentest/errprint.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 3426

. ${srcdir}/defs

cat <<\EOF >in
errprint(`Illegal arguments to forloop
')
EOF

cat <<\EOF >ok

EOF

cat <<\EOF >okerr
Illegal arguments to forloop
EOF

$M4 -d in >out 2>err
sed -e "s, ../../src/m4:, m4:," err >sederr && mv sederr err

$CMP -s out ok && $CMP -s err okerr

