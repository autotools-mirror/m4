dnl Copyright (C) 2006, 2010, 2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it
dnl with or without modifications, as long as this notice is preserved.
`currenttime' = currenttime
`ctime'       = ctime != ctime(eval(currenttime+60*60*24))
gmtime      = gmtime(currenttime)
localtime   = localtime(currenttime)
define(`q', `$1,$2,$3,$4,$5,$6,$9')dnl

currenttime
eval(currenttime+60*60*24)
localtime(eval(currenttime+60*60*24))
q(localtime(eval(currenttime+60*60*24)))
mktime      = mktime(q(localtime(eval(currenttime+60*60*24))))

%A %B %d, %Y  = strftime(`%A %B %d, %Y', currenttime)
%X on %x      = strftime(`%X on %x', currenttime)
