#!/bin/sh

# gentest/esyscmd.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 3315

. ${srcdir}/defs

cat <<\EOF >in
define(`vice', `esyscmd(grep Vice ../Makefile)')
vice
EOF

cat <<\EOF >ok

#  Ty Coon, President of Vice

EOF

$M4 -d in >out

$CMP -s out ok

