#!/bin/sh

: ${M4:=../../src/m4}

test $# -eq 1 || exit 1
FILE=`basename $1 .m4`


test -r $FILE.m4 || exit 1

if head -1 $FILE.m4 | fgrep -w 'dnl noauto' >/dev/null; then
    echo "$FILE.test cannot be generated" 1>&2
    exit 1
fi

test -d testSubDir || mkdir testSubDir

cat "$FILE.m4" > testSubDir/in
(cd testSubDir; $M4 -I.. -d "in" >out 2>err)

(

cat <<EOFEOF
#!/bin/sh

# $FILE.test is part of the GNU m4 testsuite

. \${srcdir}/defs

cat \${srcdir}/$FILE.m4 >in
EOFEOF

echo
echo 'cat <<\EOF >ok'
cat testSubDir/out
echo EOF

if [ -s testSubDir/err ]; then
    echo
    echo 'cat <<\EOF >okerr'
    sed -e "s,$M4: ,m4: ," testSubDir/err
    echo EOF
fi

echo
echo 'M4PATH=$srcdir:$srcdir/../tests $M4 -d in >out 2>err'
echo 'sed -e "s,../../src/m4: ,m4: ," err >sederr && mv sederr err'

if [ -s testSubDir/err ]; then
    echo '$CMP -s out ok && $CMP -s err okerr'
else
    echo '$CMP -s out ok'
fi
) >$FILE.test.new

if cmp -s $FILE.test.new $FILE.test; then
    echo "$FILE.test unchanged" 1>&2
    rm -f $FILE.test.new
else
    echo "creating $FILE.test" 1>&2
    mv $FILE.test.new $FILE.test
    chmod +x $FILE.test
fi

rm -f testSubDir/out testSubDir/err
