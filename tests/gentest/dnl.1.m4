#!/bin/sh

# gentest/dnl.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1857

. ${srcdir}/defs

cat <<\EOF >in
define(`foo', `Macro `foo'.')dnl A very simple macro, indeed.
foo
EOF

cat <<\EOF >ok
Macro foo.
EOF

$M4 -d in >out

$CMP -s out ok

