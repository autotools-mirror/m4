loadmodule(`stdlib')

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

