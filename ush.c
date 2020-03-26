#include <fcntl.h>
#include "defn.h"

/* Constants */ 

#define LINELEN 200000

/* Prototypes */

void processline (char *line, int infd, int outfd, int flags);
char **arg_parse(char *line, int *argcptr);

/* SIGINT handler */
void sig_handler(){
    had_signal = 1;
    kill(g_cpid, SIGINT);
}

/* Shell main */

int main (int argc, char **argv)
{
    char   buffer [LINELEN];
    int    len;
    int    fd = -1;
    int    i = 0;

    // Initialize globals
    mainargc = argc;
    mainargv = argv;
    shiftnum = 0;
    retcode = 0;

    // Registering SIGINT handler
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sigIntHandler, NULL);

    if(argc > 1)
    {
        if((fd = open(argv[1], O_RDONLY, 0)) < 0)
        {
            perror("open");
            exit(127);
        }
        if(dup2(fd, 0) < 0) // Redirects stdin to fd 
        {
            perror("dup2");
            exit(1);
        }
    }

    while (1)
    {
        had_signal = 0;
        /* prompt and get line */
        if(fd == -1)
           fprintf (stderr, "%% ");
        if (fgets (buffer, LINELEN, stdin) != buffer)
            break;

        /* Get rid of \n at end of buffer. */
        len = strlen(buffer);
        for(i = 0; i < len; i++)
        {
            if(buffer[i] == '#')
            {
                if(i == 0 || (i - 1 >= 0 && buffer[i-1] != '$'))
                    buffer[i] = 0;
            }
        }
        if (buffer[len-1] == '\n')
            buffer[len-1] = 0;

        /* Run it ... */
        processline (buffer, 0, 1, EXPAND | WAIT);
    }

    if (!feof(stdin) && !had_signal)
        perror ("read");

    return 0;		/* Also known as exit (0); */
}

char **arg_parse(char *line, int *argcptr)
{
    int arg_count = 0; // Number of arguments found in line
    int dest_i = 0, src_i = 0, arg_i = 0; // Index for dest, src, and current # of arguments located
    char **arg_list = NULL; // Records individual arguments found in line

    char *src = line; // Keeps track of text in the original line parameter
    char *dest = line; // Keeps track of edited line (without quotes)

    // Iterates through the line counting arguments
    while(src[src_i] != 0)
    {
        while(src[src_i] == ' ') src_i++;
        if(src[src_i] != 0)
        {
           // Found an arg
           arg_count++;
           // Find end of arg
           while(src[src_i] != ' ' && src[src_i] != 0)
           {
               if(src[src_i] == '"')
               {
                   src_i++;

                   // Find matching quotation
                   while(src[src_i] != '"') src_i++;
                   src_i++;
               }
               else
                   src_i++;
           }
        }
    }
    *argcptr = arg_count;
    arg_list = (char **) malloc(sizeof(char*)*(arg_count + 1));
    if(arg_list == NULL)
    {
        fprintf(stderr, "Can't allocate enough space. Try restarting the application.\n");
        return NULL;
    }
    // Adds a pointer to the beginning of each argument and a null character at the end
    // then adds the argument to arg_list
    for(src_i = 0; src[src_i] != 0; src_i++)
    {
        while(src[src_i] == ' ') src_i++, dest_i++;
        if(src[src_i] != 0)
        {
            // Found an arg
            arg_list[arg_i++] = &dest[dest_i];

            // Find end of arg
            while(src[src_i] != ' ' && src[src_i] != 0)
            {
                // Check for quoted arg start
                if(src[src_i] == '"')
                {
                    src_i++;
                    while(src[src_i] != '"')
                    {
                        if(src[src_i] == 0)
                        {
                            // No matching quotation found
                            fprintf(stderr, "Error: No matching quotation mark.\n");
                            arg_count = 0;
                            break;
                        }
                        dest[dest_i++] = src[src_i++];
                    }
                    src_i++;
                }
                else
                    dest[dest_i++] = src[src_i++];
            }
            // Place null character at the end of each arg
            dest[dest_i++] = 0;
        }
    }
    arg_list[arg_count] = NULL;
    return arg_list;
}

void processline (char *line, int infd, int outfd, int flags)
{
    int status = 0;
    int arg_count = 0;
    char **arg_list = NULL;
    char newline[LINELEN] = {0};
    char *nextPipe = NULL; // Keeps track of any pipe symbols
    int fd[2];
    int tempfd = infd;

    if(flags & EXPAND)
    {
        if(expand(line, newline, LINELEN) == 1) return; // Return if there is expansion overflow
        line = newline;
    }
    // Check for pipelines
    if(strchr(line, '|') != NULL)
    {
        while((nextPipe = strchr(line, '|')) != NULL)
        {
            // Replace pipe with null char
            *nextPipe = 0;

            // Open pipe using fd
            if(pipe(fd) < 0)
            {
                perror("pipe");
                return;
            }

            // Process command
            processline(line, tempfd, fd[1], WAIT);
            // Close pipe that we wrote on
            close(fd[1]);
            // Close pipe we read on if it wasnt param infd
            if(tempfd != infd)
                close(tempfd);
            // Move pipe we read on to next process
            tempfd = fd[0];
            // Remove what we just processed
            line = nextPipe+1;
        }
        // Process the last command
        processline(line, tempfd, outfd, WAIT);
        
        close(tempfd);
        return;
    }

    if(!had_signal)
    {
        if(!(flags & EXPAND))
            arg_list = arg_parse(line, &arg_count);
        else
            arg_list = arg_parse(newline, &arg_count);
    }

    // Return if no arguments were found, or arg is a builtin
    if (arg_count == 0 || (is_builtin = checkBuiltIn(outfd, arg_list, arg_count)) == 1)
    {
      free(arg_list);
      return;
    }
    
    /* Start a new process to do the job. */
    g_cpid = fork();
    if (g_cpid < 0) {
      /* Fork wasn't successful */
      perror ("fork");
      free(arg_list);
      return;
    }
    
    /* Check for who we are! */
    if (g_cpid == 0) {
      /* We are the child! */
      if(dup2(outfd, 1) < 0) // Redirects stdout to outfd 
      {
          perror("dup2");
          exit(1);
      }
      if(dup2(infd, 0) < 0) // Redirects stdin to infd 
      {
          perror("dup2");
          exit(1);
      }
      execvp(arg_list[0], arg_list);
      /* execlp reurned, wasn't successful */
      perror ("exec");
      fclose(stdin);  // avoid a linux stdio bug
      exit (127);
    }
    
    /* Have the parent wait for child to complete */
    if(flags & WAIT)
    {
        if (waitpid (g_cpid, &status, 0) < 0) {
          /* Wait wasn't successful */
          perror ("wait");
        }
    }

    if (WIFEXITED(status))
        retcode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
    {
        retcode = 128 + WTERMSIG(status);
        if (WTERMSIG(status) != SIGINT)
        {
            dprintf(outfd, "%s", strsignal(WTERMSIG(status)));
            if (WCOREDUMP(status))
                dprintf(outfd, " (core dumped)");
            dprintf(outfd, "\n");
        }
    }
    else
        retcode = status;

    free(arg_list);
    return;
}
