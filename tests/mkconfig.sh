#!/bin/sh
env >/tmp/env
sed -n -e '/^#define \(ENABLE_.*\) \(.*\)$/s//define(\1, \2)dnl/p' -e '/^#define \(WITH_.*\) \(.*\)$/s//define(\1, \2)dnl/p' config.h > tests/config.m4
sed -n -e '/^#define \(ENABLE_.*\) \(.*\)$/s//\1=\2/p' -e '/^#define \(WITH_.*\) \(.*\)$/s//\1=\2/p' config.h > tests/config.sh
