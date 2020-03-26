#include "defn.h"

int expand(char *orig, char *new, int newsize)
{
    int o_i = 0, n_i = 0, arg_i = 0; // Index variables for orig, new, and the new arg respectively
    int script_arg = 0; // Integer to store file arg parameters
    int isTerminated = 0; // Tells whether a string is already null terminated
    char *temp_arg = NULL; // Beginning of the new arg
    char *suffix = NULL; // Points to context characters for wildcard
    char temp_num[20]; // Stores ascii character to represent an integer
    int fileFound = 0; // Represents boolean that tracks if wildcard has matched a file
    int fd[2];
    int status = 0;

    DIR *dirp; // Pointer to working directory
    struct dirent *dp; // Pointer to file info found in dirp
    // Copies arguments into temp_arg, moves orig index to the end of the arg
    // Then copies temp_arg into new
    while(!had_signal && orig[o_i] && n_i < newsize)
    {
        int oparen_count = 0, cparen_count = 0;
            arg_i = 0;
        if (orig[o_i] == '(')
            oparen_count++;
        if (orig[o_i] == ')')
            cparen_count++;
        // Wildcard expansion w/o context
        if (orig[o_i] == '*' && (orig[o_i+1] == ' ' || orig[o_i+1] == 0)
                             && (orig[o_i-1] == ' ' || orig[o_i-1] == 0))
        {
            o_i++;
            if((dirp = opendir(".")) == NULL)
            {
                perror("open");
                return 1;
            }
            while((dp = readdir(dirp)) != NULL && n_i < newsize)
            {
                if(dp->d_name[0] != '.')
                {
                    n_i += sprintf(&new[n_i], "%s ", dp->d_name);
                    if(n_i >= newsize)
                    {
                        fprintf(stderr, "Error: Expansion overflow\n");
                        return 1;
                    }
                }
            }
            closedir(dirp);
        }
        // Wildcard w/ context characters
        else if (orig[o_i] == '*' && (orig[o_i+1] != ' ' || orig[o_i+1] != 0)
                                  && (orig[o_i-1] == ' ' || orig[o_i-1] == 0))
        {
            o_i++;
            // Save context characters
            suffix = &orig[o_i];
            // Find end of context characters
            while(orig[o_i] != ' ' && orig[o_i] != 0) o_i++, arg_i++;
            if(orig[o_i] == 0)
                isTerminated = 1;
            else
                orig[o_i] = 0;
            if((dirp = opendir(".")) == NULL)
            {
                perror("open");
                return 1;
            }
            while((dp = readdir(dirp)) != NULL && n_i < newsize)
            {
                temp_arg = dp->d_name;
                int diff = strlen(temp_arg)-strlen(suffix);
                if(diff >= 0 && (strcmp(&temp_arg[diff], suffix) == 0))
                {
                    fileFound = 1;
                    n_i += sprintf(&new[n_i], "%s ", temp_arg);
                    if(n_i >= newsize)
                    {
                        fprintf(stderr, "Error: Expansion overflow\n");
                        return 1;
                    }
                }
            }
            if(!fileFound)
                o_i -= strlen(suffix)+1;
            if(!isTerminated)
                orig[o_i] = ' ';
            closedir(dirp);
        }
        else if (orig[o_i] == '\\' && orig[o_i+1] == '*')
            o_i++;

        // Variable expansion
        else if (orig[o_i] == '$')
        {
            // Command line expansion
            if(orig[o_i+1] == '(')
            {
		        oparen_count++;
                o_i += 2;
                // Found an arg
                temp_arg = &orig[o_i];
                // Find end of arg
                while(orig[o_i])
                {
		            if(orig[o_i] == '(')
			            oparen_count++;
		            if(orig[o_i] == ')')
			            cparen_count++;
                    if(orig[o_i] == 0 || oparen_count == cparen_count)
                        break;
                    o_i++;
                }
		        cparen_count++;
                if(orig[o_i] == ')')
                {
                    int disp = 0, start = 0, i = 0;
		            cparen_count++;
                    // Temporary change to orig
                    orig[o_i] = 0;
                    if(pipe(fd) < 0)
                    {
                        perror("pipe");
                        return 1;
                    }
                    processline(temp_arg, fd[0], fd[1], EXPAND);
                   
		            start = n_i;
		            close(fd[1]);
                    while((disp = read(fd[0], &new[n_i], newsize - n_i)) > 0)
                    {
                        n_i += disp;
                        if(n_i >= newsize)
                        {
                            fprintf(stderr, "Error: Expansion overflow\n");
                            return 1;
                        }

		                for(i = start; i <= n_i; i++)
		                {
		                    if(new[i] == '\n')
			                {
			                    if(i == n_i - 1)
				                    n_i -= 1;
			                    else
			                        new[i] = ' ';
			                }
		                }
                    }
                    close(fd[0]);

                    if(!is_builtin && waitpid(-1, &status, WNOHANG) < 0)
                    {
                        perror("wait");
                        return 1;
                    }
                    if(WIFEXITED(status))
                        retcode = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status))
                        retcode = 128 + WEXITSTATUS(status);
                    else
                        retcode = status;
                    
                    // Add env variable to new
                    if(temp_arg == NULL)
                        temp_arg = "";
                    // Change orig back to how it was
                    orig[o_i++] = ')';
                    continue;
                }else{
		            fprintf(stderr, "Error: No matching )\n");
		            return 1;
		        }
            }
            // Env variables
            if(orig[o_i+1] == '{')
            {
                o_i += 2;
                // Found an arg
                temp_arg = &orig[o_i];
                // Find end of arg
                while(orig[o_i] != '}')
                {
                    if(orig[o_i] == 0)
                    {
                        fprintf(stderr, "Error: No matching }\n");
                        return 1;
                    }
                    else
                        o_i++;
                }
                if(orig[o_i] == '}')
                {
                    // Temporary change to orig
                    orig[o_i] = 0;
                    temp_arg = getenv(temp_arg);
                    // Add env variable to new
                    if(temp_arg == NULL)
                        temp_arg = "";
                    // Change orig back to how it was
                    orig[o_i++] = '}';
                }
            }
            // Process ID
            else if(orig[o_i+1] == '$')
            {
                o_i += 2;
                snprintf(temp_num, 20, "%ld", (long)getpid());
                temp_arg = temp_num;
            }
            // Number of arguments
            else if(orig[o_i+1] == '#')
            {
                o_i += 2;
                if(mainargc - 1 > shiftnum)
                {
                    snprintf(temp_num, 20, "%d", mainargc - 1 - shiftnum);
                    temp_arg = temp_num;
                }
                else
                    temp_arg = "1";
            }
            // Ret code
            else if(orig[o_i+1] == '?')
            {
                o_i += 2;
                snprintf(temp_num, 20, "%d", retcode);
                temp_arg = temp_num;
            }
            // File arguments
            else if(isdigit(orig[o_i+1]))
            {
                // Found arg
                temp_arg = &orig[o_i+1];
                // Find end of arg
                while(isdigit(orig[o_i+1])) o_i++;
                script_arg = atoi(temp_arg);
                if(mainargc > 1)
                {
                    o_i++;
                    if(script_arg == 0)
                        temp_arg = mainargv[1];
                    else if(script_arg + shiftnum + 1 >= mainargc || script_arg + shiftnum < 0)
                        temp_arg = "";
                    else
                        temp_arg = mainargv[script_arg+shiftnum+1];
                }
                else // In an interactive session
                {
                    o_i++;
                    if(script_arg != 0)
                        temp_arg = "";
                    else
                        temp_arg = mainargv[0];
                }
            }
            else
                temp_arg = "";
            arg_i = 0;
            // Copy temp_arg into new
            while(temp_arg[arg_i])
            {
                if(n_i >= newsize)
                {
                    fprintf(stderr, "Error: Expansion overflow\n");
                    return 1;
                }
                new[n_i++] = temp_arg[arg_i++];
            }
        }
        new[n_i++] = orig[o_i++];
    }
    new[n_i] = 0;
    return 0;
}
