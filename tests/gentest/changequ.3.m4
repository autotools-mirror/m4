#!/bin/sh

# gentest/changequ.3.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1919

. ${srcdir}/defs

cat <<\EOF >in
define(`foo', `Macro `FOO'.')
changequote(, )
foo
`foo'
EOF

cat <<\EOF >ok


Macro `FOO'.
`Macro `FOO'.'
EOF

$M4 -d in >out

$CMP -s out ok

