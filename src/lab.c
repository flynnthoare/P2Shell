
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "lab.h"



/**
* @brief Set the shell prompt. This function will attempt to load a prompt
* from the requested environment variable, if the environment variable is
* not set a default prompt of "shell>" is returned.  This function calls
* malloc internally and the caller must free the resulting string.
*
* @param env The environment variable
* @return const char* The prompt
*/
char *get_prompt(const char *env) {
    if (env == NULL) {  
        return strdup("shell>");  
    }
    
    char *prompt = getenv(env);     // we make a copy of this because content can be corrupted when getenv called again

    if (prompt) {
        return strdup(prompt);  
    }
    return strdup("shell>");  // Return default prompt
}

/**
* Changes the current working directory of the shell. Uses the linux system
* call chdir. With no arguments the users home directory is used as the
* directory to change to.
*
* @param dir The directory to change to
* @return  On success, zero is returned.  On error, -1 is returned, and
* errno is set to indicate the error.
*/
//Simplified change_dir function to fix failing tests post code review
int change_dir(char **dir) {
    if (!dir[1] || !dir) {
        // Retrieve the home directory of user
        struct passwd *pw = getpwuid(getuid());
        
        if (!pw || !pw->pw_dir) {
            perror("getpwuid"); 
            return -1; 
        }
        
        // Change to the user's home directory
        return chdir(pw->pw_dir);
    }
    
    // Change to the specified directory
    return chdir(dir[1]);
}


/**
* @brief Convert line read from the user into to format that will work with
* execvp. We limit the number of arguments to ARG_MAX loaded from sysconf.
* This function allocates memory that must be reclaimed with the cmd_free
* function.
*
* @param line The line to process
*
* @return The line read in a format suitable for exec
*/
char **cmd_parse(const char *line) {
    
    if (!line) {
        fprintf(stderr, "cmd_parse: Received NULL input\n");
        return NULL;
    }

    // copy user input so we can also access the original string
    char *input = strdup(line);

    //check for failed malloc
    if (!input) {
        fprintf(stderr, "Memory allocation failed for input.\n");
        return NULL;
    }
    
    // points to an array of memory addresses (each of those addresses points to a string)
    long arg_max = sysconf(_SC_ARG_MAX);
    char **args = malloc(arg_max * sizeof(char *));  // allocate memory for 64 args
    
    // check for failed malloc
    if (!args) {
        fprintf(stderr, "Memory allocation failed for arguments array.\n");
        free(input);
        return NULL;
    }

    int position = 0;
    
    // break input into tokens using delimiter (space)
    char *token = strtok(input, " ");               // use strtok_r for thread safe version
    while (token != NULL) {
        args[position++] = strdup(token);   // copy tokens to args array
        token = strtok(NULL, " ");          // sets token to next token in input
    }

    //handle if too many args
    if (position >= arg_max - 1) {
        fprintf(stderr, "Too many arguments (limit reached)\n");
        for (int i = 0; i < position; i++) {
            free(args[i]);
        }
        free(args);
        free(input);
        return NULL;
    }

    if (position == 0) {  // If no arguments were found
        fprintf(stderr, "cmd_parse: No args found.\n");
        free(input);
        free(args);
        return NULL;  
    }

    // add NULL termination to args list
    args[position] = NULL;

    free(input);
    return args;
}

/**
* @brief Free the line that was constructed with parse_cmd
*
* @param line the line to free
*/
void cmd_free(char **line) {
    if (!line) return;
    
    int i = 0;
    while (line[i] != NULL) {
        free(line[i]);
        i++;
    }
    free(line);
}

/**
* @brief Trim the whitespace from the start and end of a string.
* For example "   ls -a   " becomes "ls -a". This function modifies
* the argument line so that all printable chars are moved to the
* front of the string
*
* @param line The line to trim
* @return The new line with no whitespace
*/
char *trim_white(char *line) {
    if (!line) return NULL;  // NULL check

    char *start = line;
    while (isspace((unsigned char)*start)) start++; // Move past leading spaces

    if (*start == '\0') { 
        *line = '\0'; // If all spaces
        return line;
    }

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--; // Move back past trailing spaces

    *(end + 1) = '\0';  // Null-terminate after last char (thats not a space)

    if (start != line) {
        memmove(line, start, end - start + 2);  // Shift trimmed string in place
    }

    return line;
}

/**
* @brief Takes an argument list and checks if the first argument is a
* built in command such as exit, cd, jobs, etc. If the command is a
* built in command this function will handle the command and then return
* true. If the first argument is NOT a built in command this function will
* return false.
*
* @param sh The shell
* @param argv The command to check
* @return True if the command was a built in command
*/
bool do_builtin(struct shell *sh, char **argv) {

    if (!argv || !argv[0]) {
        cmd_free(argv);
        return false;
    }

    //exit
    if (strcmp(argv[0], "exit") == 0) {
        printf("Exiting shell normally.\n");
        
        //free resources then exit
        sh_destroy(sh);
        exit(0);    // dont need to return anything since program terminated
    }

    
    //cd
    if (strcmp(argv[0], "cd") == 0) {
        if (change_dir(argv) != 0) {        //edited to simplify post codereview
            return false;
        }

        return true;
    }

    //history
    if (strcmp(argv[0], "history") == 0) {
        HIST_ENTRY **history_entries = history_list();  // Get the history list
        if (history_entries) {
            int i = 0;
            while (history_entries[i] != NULL) {
                printf("%d: %s\n", i + 1, history_entries[i]->line);
                i++;
            }
        } else {
            printf("No command history available.\n");
        }
        return true;
    }

    return false;  // Always return false if not built in
}

/**
* @brief Initialize the shell for use. Allocate all data structures
* Grab control of the terminal and put the shell in its own
* process group. NOTE: This function will block until the shell is
* in its own program group. Attaching a debugger will always cause
* this function to fail because the debugger maintains control of
* the subprocess it is debugging.
*
* @param sh
*/
void sh_init(struct shell *sh) {

    sh->shell_terminal = STDIN_FILENO;  // standard input (keyboard input)
    sh->shell_is_interactive = isatty(sh->shell_terminal);

    // set up process groups if interactive
    if (sh->shell_is_interactive) {
        sh->shell_pgid = getpid();  // get pid of shell and make the pgid for shell

        // assign shell to its own process group
        if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0) {  // setpgid(process id, group id)
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        //take control of the terminal by telling system which terminal should be controlled by which pg
        if (tcsetpgrp(sh->shell_terminal, sh->shell_pgid) < 0) {    //tcsetpgrp(file descriptor, group id)
            perror("Failed to take control of the terminal");
            exit(1);
        }

        // save current terminal settings
        if (tcgetattr(sh->shell_terminal, &sh->shell_tmodes) < 0) { //tcgetattr(file descriptor, pointer to struct termios - stores terminal settings)
            perror("Failed to get terminal attributes");
            exit(1);
        }   

        // IGNORE SIGNALS
        signal(SIGINT, SIG_IGN);   // Ignore Ctrl+C
        signal(SIGQUIT, SIG_IGN);  // Ignore Ctrl+'\'
        signal(SIGTSTP, SIG_IGN);  // Ignore Ctrl+Z
        signal(SIGTTIN, SIG_IGN);  // Ignore background process input
        signal(SIGTTOU, SIG_IGN);  // Ignore background process output

    }

    // Initialize the command history feature
    using_history();

    //set up shell prompt
    sh->prompt = get_prompt(getenv("MY_PROMPT"));

}

/**
* @brief Destroy shell. Free any allocated memory and resources and exit
* normally.
*
* @param sh
*/
void sh_destroy(struct shell *sh) {

    // Free memory for the prompt
    if (sh->prompt) {
        free(sh->prompt);
    }

    // Restore terminal settings if necessary
    if (sh->shell_is_interactive) {
        if (tcsetattr(sh->shell_terminal, TCSADRAIN, &sh->shell_tmodes) < 0) {
            perror("Failed to restore terminal settings");
        }
    }
    clear_history();
}


/**
* @brief Parse command line args from the user when the shell was launched
*
* @param argc Number of args
* @param argv The arg array
*/
void parse_args(int argc, char **argv) {
    int opt;

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch(opt) {
            case 'v':
                //print the version and then exit
                printf("Shell Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                exit(0);
            case '?':   //unknown option
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                exit(1);    
        }
    }
}
