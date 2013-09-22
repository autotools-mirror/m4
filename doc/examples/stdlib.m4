dnl Copyright (C) 2006, 2010, 2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it
dnl with or without modifications, as long as this notice is preserved.
load(`stdlib')

`getenv - 'getenv(PATH)

setenv TEST=??? setenv(`TEST', `???')
getenv TEST - getenv(`TEST')

setenv TEST=Second test setenv(`TEST', `Second test')
getenv TEST - getenv(`TEST')

unsetenv TEST unsetenv(`TEST')
getenv TEST - getenv(`TEST')



`getlogin - 'getlogin
`getcwd = 'getcwd
`getpid - 'getpid
`getppid - 'getppid

syscmd(`ps ajx|grep m4')

`getuid - 'getuid

user root - getpwnam(`root')
user sync - getpwnam(`sync')
user rene - getpwnam(`rene')

uid 5 - getpwuid(5)
me - getpwuid(getuid)

`hostname = 'hostname

`rand' - rand,rand,rand,rand
`srand' srand
`rand' - rand,rand,rand,rand
`srand' srand
`rand' - rand,rand,rand,rand

`uname - ' uname
