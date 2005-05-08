# Extract all examples from the manual source.            -*- AWK -*-

# This file is part of GNU M4
# Copyright 1992, 2000, 2001 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

# This script is for use with any New AWK.

BEGIN {
  seq = -1;
  status = 0;
  print "# This file is part of the GNU m4 test suite.  -*- Autotest -*-";
  # I don't know how to get this file's name, so it's hard coded :(
  print "# Do not edit by hand, it was generated by generate.awk.";
  print "#";
  print "# Copyright 1992, 2000, 2001 Free Software Foundation, Inc.";
  print ;
  print "AT_BANNER([Documentation examples.])";
  print ;
  # stop spurious warnings in the erenamesyms checks
  print "m4_pattern_allow([^m4_(m4|erenamesyms|)$])"
  print ;
}

/^@node / {
  if (seq > 0)
    print "AT_CLEANUP";

  split ($0, tmp, ",");
  node = substr(tmp[1], 7);
  seq = 0;
}

/^@comment file: / {
  file = $3;
}

/^@comment ignore$/ {
  getline;
  next;
}

/^@comment status: / {
  status = $3;
}

/^@example$/, /^@end example$/ {
  if (seq < 0)
    next;

  if ($0 ~ /^@example$/)
    {
      if (seq == 0)
        new_group(node);
      if (!file)
        seq++;
      printf ("# %s:%d\n", FILENAME, NR)
      next;
    }

  if ($0 ~ /^@end example$/)
    {
      if (file != "")
        {
           if (output || error)
             {
               fatal("while getting file " file      \
		     " found output = " output ","  \
		     " found error = " error);
             }
           input = normalize(input);
           printf ("AT_DATA([[%s]],\n[[%s]])\n\n", file, input);
        }
      else
        {
           new_test(input, status, output, error);
           status = 0;
        }
      file = input = output = error = "";
      next;
    }

  if ($0 ~ /^\^D$/)
    next;

  if ($0 ~ /^@result\{\}/)
    output = output $0 "\n";
  else if ($0 ~ /^@error\{\}/)
    error = error $0 "\n";
  else
    input = input $0 "\n";
}

END {
  if (seq > 0)
    print "AT_CLEANUP";
}

# We have to handle CONTENTS line per line, since anchors in AWK are
# referring to the whole string, not the lines.
function normalize(contents,    i, lines, n, line, res) {
  # Remove the Texinfo tags.
  n = split (contents, lines, "\n");
  # We don't want the last field which empty: it's behind the last \n.
  for (i = 1; i < n; ++i)
    {
      line = lines[i];
      gsub (/^@result\{\}/, "", line);
      gsub (/^@error\{\}/,  "", line);
      gsub ("@[{]", "{", line);
      gsub ("@}", "}", line);
      gsub ("@@", "@", line);
      gsub ("@comment.*", "@\\&t@", line);

      # Some of the examples have improperly balanced square brackets.
      gsub ("[[]", "@<:@", line);
      gsub ("[]]", "@:>@", line);

      res = res line "\n";
    }
  return res;
}

function new_group(node) {
  banner = node ". ";
  gsub (/./, "-", banner);
  printf ("\n\n");
  printf ("## %s ##\n", banner);
  printf ("## %s.  ##\n", node);
  printf ("## %s ##\n", banner);
  printf ("\n");
  printf ("AT_SETUP([[%s]])\n", node);
  printf ("AT_KEYWORDS([[documentation]])\n\n");
}

function new_test(input, status, output, error) {
  input = normalize(input);
  output = normalize(output);
  error = normalize(error);

  printf ("AT_DATA([[input.m4]],\n[[%s]])\n\n", input);
  # Some of these tests `include' files from tests/.
  printf ("AT_CHECK_M4([[input.m4]], %s,", status);
  if (output)
    printf ("\n[[%s]]", output);
  else
    printf (" []");
  if (error)
    printf (",\n[[%s]])", error);
  else
    printf (")");
  printf ("\n\n");
}

function fatal(msg) {
  print "generate.awk: " FILENAME ":" NR ": " msg > "/dev/stderr"
  exit 1
}
