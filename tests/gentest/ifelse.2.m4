#!/bin/sh

# gentest/ifelse.2.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1497

. ${srcdir}/defs

cat <<\EOF >in
ifelse(foo, bar, `third', gnu, gnats, `sixth', `seventh')
EOF

cat <<\EOF >ok
seventh
EOF

$M4 -d in >out

$CMP -s out ok

