#!/bin/sh

# gentest/indir.1.m4 is part of the GNU m4 testsuite
# generated from example in ../doc/m4.texinfo line 1369

. ${srcdir}/defs

cat <<\EOF >in
define(`$$internal$macro', `Internal macro (name `$0')')
$$internal$macro
indir(`$$internal$macro')
EOF

cat <<\EOF >ok

$$internal$macro
Internal macro (name $$internal$macro)
EOF

$M4 -d in >out

$CMP -s out ok

