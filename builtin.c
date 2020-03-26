#include "defn.h"
#include <pwd.h>
#include <grp.h>
#include <time.h>

int checkBuiltIn(int tempofd, char **argv, int argc);
void ush_exit(char **argv, int argc);
void ush_envset(char **argv, int argc);
void ush_envunset(char **argv, int argc);
void ush_cd(char **argv, int argc);
void ush_shift(char **argv, int argc);
void ush_unshift(char **argv, int argc);
void ush_sstat(char **argv, int argc);

typedef void (*funcptr) (char **, int);

// Parallel arrays to match user command to function
static char* built_ins[] = { "exit", "envset", "envunset", "cd", "shift", "unshift", "sstat", 0 };
static funcptr flist[] = {ush_exit, ush_envset, ush_envunset, ush_cd, ush_shift, ush_unshift, ush_sstat};
int outfd;

int checkBuiltIn(int tempofd, char **argv, int argc)
{
    char **temp = built_ins;
    int isBuiltIn = 0;
    outfd = tempofd;
    int b_i = 0; // Keeps track of which command was used, and which function should be called

    while(*temp != 0)
    {
        // If first arg in argv matches a string in built_ins
        // Then argv represents a built in command
        if(strcmp(*argv, *temp++) == 0)
        {
            isBuiltIn = 1;
            break;
        }
        b_i++;
    }
    if(isBuiltIn)
        flist[b_i](argv, argc);
    return isBuiltIn;
}

void ush_exit(char **argv, int argc)
{
    if(argc > 1)
    {
        int exit_code = atoi(argv[1]);
        if(exit_code <= 255 && exit_code >= 0) // Checks for valid exit code
        {
            exit(exit_code);
            retcode = 1;
        }
    }
    // Otherwise exits with 0
    exit(0);
}

void ush_envset(char **argv, int argc)
{
    if(argc < 3)
    {
        fprintf(stderr, "envset: Both a name and value are needed to set an environment variable.\n");
        retcode = 1;
    }
    else
        setenv(argv[1], argv[2], 1); // Third argument is set to 1 for overwrite
}

void ush_envunset(char **argv, int argc)
{
    if(argc != 2)
    {
        fprintf(stderr, "envunset: Enter a variable name to unset.\n");
        retcode = 1;
    }
    else
        unsetenv(argv[1]);
}

void ush_cd(char **argv, int argc)
{
    char *dest = NULL;
    if(argc < 2)
    {
        if((dest = getenv("HOME")) == NULL)
        {
            retcode = 1;
            perror("cd");
            return;
        }
    }
    else
        dest = argv[1];
    if(chdir(dest) < 0)
    {
        retcode = 1;
        fprintf(stderr, "cd: No such file or directory\n");
    }
}

void ush_shift(char **argv, int argc)
{
    // temp represents shift amount
    // fileargs is number of accessible parameters passed into main
    int temp, fileargs = mainargc-1-shiftnum;
    // Shift by 1 unless told otherwise
    if(argc != 2)
        temp = 1;
    else
    {
        temp = atoi(argv[1]);
        if(temp < 0)
        {
            retcode = 1;
            fprintf(stderr, "shift: Use unshift to shift backwards.\n");
            return;
        }
    }
    // Ensure shift is within limits
    if(temp + shiftnum <= fileargs)
        shiftnum += temp;
    else
    {
        retcode = 1;
        fprintf(stderr, "shift: Not enough arguments to shift by %d.\n", temp);
    }
}
void ush_unshift(char **argv, int argc)
{
    // temp represents amount to unshift by
    int temp;
    // Unshift completely by default
    if(argc < 2)
        shiftnum = 0;
    else
    {
        temp = atoi(argv[1]);
        if(temp < 0)
        {
            retcode = 1;
            fprintf(stderr, "unshift: Use shift to shift forwards");
            return;
        }
        // Ensure unshift is within limits
        if(temp < shiftnum)
            shiftnum -= temp;
        else
        {
            retcode = 1;
            fprintf(stderr, "unshift: Not enough arguments to unshift by %d.\n", temp);
        }
    }
}

void ush_sstat(char **argv, int argc)
{
    struct stat fileStat;
    struct passwd *pwd = NULL;
    struct group *grp = NULL;
    time_t modified;
    mode_t mode;

    int file_i = 1;
    
    if(argc < 2)
    {
        retcode = 1;
        fprintf(stderr, "sstat: No files to stat.\n");
    }
    else
    {
        while(file_i < argc)
        {
            if(stat(argv[file_i],&fileStat) < 0)
            {
                retcode = 1;
                fprintf(stderr, "sstat: No such file or directory.\n");
                return;
            }
            // File name
            dprintf(outfd, "%s ", argv[file_i]);
            // Username
            if((pwd = getpwuid(fileStat.st_uid)) != NULL)
                dprintf(outfd, "%s ", pwd->pw_name);
            else
                dprintf(outfd, "%d ", fileStat.st_uid);
            // Group name
            if((grp = getgrgid(fileStat.st_gid)) != NULL)
                dprintf(outfd, "%s ", grp->gr_name);
            else
                dprintf(outfd, "%d ", fileStat.st_gid);
            // File permissions
            mode = fileStat.st_mode;
            dprintf(outfd, (S_ISDIR(mode)) ? "d" : "-");
            dprintf(outfd, (mode & S_IRUSR) ? "r" : "-");
            dprintf(outfd, (mode & S_IWUSR) ? "w" : "-");
            dprintf(outfd, (mode & S_IXUSR) ? "x" : "-");
            dprintf(outfd, (mode & S_IRGRP) ? "r" : "-");
            dprintf(outfd, (mode & S_IWGRP) ? "w" : "-");
            dprintf(outfd, (mode & S_IXGRP) ? "x" : "-");
            dprintf(outfd, (mode & S_IROTH) ? "r" : "-");
            dprintf(outfd, (mode & S_IWOTH) ? "w" : "-");
            dprintf(outfd, (mode & S_IXOTH) ? "x " : "- ");
            // Number of links
            dprintf(outfd, "%d ", (int)fileStat.st_nlink);
            // Size in bytes
            dprintf(outfd, "%d ", (int)fileStat.st_size);
            // Last modified time
            modified = fileStat.st_mtime;
            dprintf(outfd, "%s", asctime(localtime(&modified)));
            file_i++;
        }
    }
}
