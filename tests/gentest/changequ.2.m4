#!/bin/sh

# gentest/changequ.2.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1907

. ${srcdir}/defs

cat <<\EOF >in
changequote([[, ]])
define([[foo]], [[Macro [[[foo]]].]])
foo
EOF

cat <<\EOF >ok


Macro [foo].
EOF

$M4 -d in >out

$CMP -s out ok

