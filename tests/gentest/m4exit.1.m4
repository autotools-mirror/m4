#!/bin/sh

# gentest/m4exit.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 3474

. ${srcdir}/defs

cat <<\EOF >in
define(`fatal_error', `errprint(`m4: '__file__: __line__`: fatal error: $*
')m4exit(1)')
fatal_error(`This is a BAD one, buster')
EOF

cat <<\EOF >ok

EOF

cat <<\EOF >okerr
m4: in: 3: fatal error: This is a BAD one, buster
EOF

$M4 -d in >out 2>err
sed -e "s, ../../src/m4:, m4:," err >sederr && mv sederr err

$CMP -s out ok && $CMP -s err okerr

