#!/bin/sh

# gentest/substr.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 2891

. ${srcdir}/defs

cat <<\EOF >in
substr(`gnus, gnats, and armadillos', 6)
substr(`gnus, gnats, and armadillos', 6, 5)
EOF

cat <<\EOF >ok
gnats, and armadillos
gnats
EOF

$M4 -d in >out

$CMP -s out ok

