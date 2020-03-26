#ifndef DEFN
#define DEFN
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define EXPAND 1
#define WAIT 2

int mainargc, shiftnum, retcode, is_builtin, had_signal;
pid_t g_cpid;
char **mainargv;
int expand(char *orig, char *new, int newsize);
int checkBuiltIn(int outfd, char **argv, int argc);
void processline(char*, int, int, int);

#endif
