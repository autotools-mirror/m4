#!/bin/sh
env >/tmp/env
sed -n -e '/^#define \(\(WITH\|ENABLE\)_.*\) \(.*\)$/s//define(\1, \3)dnl/p' config.h > tests/config.m4
sed -n -e '/^#define \(\(WITH\|ENABLE\)_.*\) \(.*\)$/s//\1=\3/p' config.h > tests/config.sh
