divert(-1)
define(`HOST', `localhost')
define(`TMP', maketemp(`/tmp/hejXXXXXX'))
syscmd(`grep ' HOST ` /etc/hosts | awk "{print \$1}"'  > TMP)
define(`IP', include(TMP))
syscmd(`rm -f' TMP)
divert
IP
