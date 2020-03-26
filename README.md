# MicroShell - CS347

A simple UNIX/Linux shell implemented in C. This project began as an assignment for my CS undergraduate degree.

# Features

- Builtin commands: envset/envunset, cd, shift/unshift, sstat, exit (See builtin.c)
- Environment variables (e.g. `${variable}`)
- Process piping (e.g. `ls -l | grep "keyword"`) and command expansion (e.g. `echo $(ls)`)
- $? returns the exit value of the last command

# Credits

A huge thank you to Phil Nelson for providing the skeleton for this code as well as the many hours of lecture and discussion he provided to help me understand this material.

