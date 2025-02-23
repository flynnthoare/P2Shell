
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
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
    printf("get_prompt() called\n");
    if (env) {
        return strdup(env);
    }
    return strdup("shell> ");  // Return a dummy prompt
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
int change_dir(char **dir) {
    printf("change_dir() called\n");
    return 0;  // Always return success
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
    printf("cmd_parse() called\n");

    // copy user input so we can also access the original string
    char *input = strdup(line);
    //check for failed malloc
    if (!input) {
        fprintf(stderr, "Memory allocation failed for input.\n");
        return NULL;
    }
    
    // points to an array of memory addresses (each of those addresses points to a string)
    char **args = malloc(64 * sizeof(char *));  // allocate memory for 64 args
    // check for failed malloc
    if (!args) {
        fprintf(stderr, "Memory allocation failed for arguments array.\n");
        free(input);
        return NULL;
    }

    int position = 0;
    
    // break input into tokens using delimiter (space)
    char *token = strtok(input, " ");
    while (token != NULL) {
        args[position++] = strdup(token);   // copy tokens to args array
        token = strtok(NULL, " ");          // sets token to next token in input
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
    printf("trim_white() called\n");
    return line;  // Return input unmodified
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
    printf("do_builtin() called\n");
    return false;  // Always return false
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
    printf("sh_init() called\n");

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
    }

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
    printf("sh_destroy() called\n");

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
