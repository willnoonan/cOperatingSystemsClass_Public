// C Program to design a shell in Linux
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <sys/fcntl.h>
#include <ctype.h>

/*
 * Author: William Noonan
 *         CSCI 5362 - Operating Systems
 *         10 Feb 2022
 * 
 * Implements a simple shell in C. All requirements satisifed, and most of the optional
 * ones (redirect of stdout not working perfectly).
 * 
 */


#define MAX_LINE  80        /* The maximum length command */
char  *prevbuffer;  /* stores the previous command */

char *copyStr(char *str) {
    char *cp = malloc(strlen(str) + 1);
    if ( cp == NULL ){			/* or die	*/
        fprintf(stderr,"no memory\n");
        exit(1);
    }
    strcpy(cp, str);
    return cp;
}


char *makestring( char *buf )
{
/* working
 * trim off newline and create storage for the string
 */
//	char	*cp, *malloc();
    char	*cp;

    buf[strlen(buf)-1] = '\0';		/* trim newline	*/
    cp = malloc( strlen(buf)+1 );		/* get memory	*/
    if ( cp == NULL ){			/* or die	*/
        fprintf(stderr,"no memory\n");
        exit(1);
    }
    strcpy(cp, buf);		/* copy chars	*/
    return cp;			/* return ptr	*/
}


char* s_trim(char *str)
{
    char *end;
    while(isspace((unsigned char)* str)) str++; // trim leading
    end = str+strlen(str)-1;
    while(end>str && isspace((unsigned char)* end)) end--;
    *(end +1) = '\0';
    return (str);
}


char **str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
    }

    return result;
}

int argBufEndsInAmpersand( char *string )
{
    char *cp = copyStr(string);
    string = strrchr(cp, '&');
    if( string != NULL ) {
        if ( strcmp(string, "&") == 0)
            return 1;
    }
    return 0;
}

int getNumArgs(char *buf) {
    /* returns number of args in buffer */
    char *cp = copyStr(buf);
    char **tokens = str_split(cp, ' ');
    if (tokens)
    {
        int i;
        for (i = 0; *(tokens + i); i++)
        {
            free(*(tokens + i));
        }
        free(tokens);
        return i;
    }
    return 0;
}

void add_history(char *buf) {
    /* stores current buffer */
    prevbuffer = malloc(strlen(buf) + 1 );
    strcpy(prevbuffer, buf);
}


// Function where the system command is executed
void execArgs(char *argbuf)
{

    char *cp = copyStr(argbuf);
    int numargs = getNumArgs(argbuf);
    char **arglist = str_split(cp, ' ');

    int wait_for_child = 1;
    if (argBufEndsInAmpersand(argbuf)) {
        /* This assumes there is a space between the last arg and '&' */
        wait_for_child = 0;
        arglist[numargs - 1] = NULL;
    }

    // Forking a child
    int pid, exitstatus;
    pid = fork();

    if (pid == -1) {
        printf("\nFailed forking child..");
        return;
    } else if (pid == 0) {
        if (execvp(arglist[0], arglist) < 0) {
            printf("\nCould not execute command..");
        }
        exit(0);
    } else {
        // waiting for child to terminate
        if (wait_for_child) {
            while (wait(&exitstatus) != pid) { ; }
        } else {
//            wait(0);  // this is the only way I could get the prompt to print again,
                        // but not sure if it waits for the child, that's why I commented it
        }
        return;
    }
}


void execArgsRedirectStdOut(char *argbuf) {
    /* works but very slow
     * But file also appears immediately after exiting the shell
     * */
    char *cp = copyStr(argbuf);
    char **split = str_split(cp, '>');
    char *args = split[0];
    char *fileargs = s_trim(split[1]);

//    char *res = s_trim(fileargs);

    int numfileargs = getNumArgs(fileargs);
    char **filearglist = str_split(fileargs, ' ');
    char **arglist = str_split(args, ' ');

    int wait_for_child = 1;
    if (argBufEndsInAmpersand(argbuf)) {
        /* This assumes there is a space between the last arg and '&' */
        wait_for_child = 0;
        filearglist[numfileargs - 1] = NULL;
    }

    char *file = filearglist[0];

    int pid, exitstatus;

    pid = fork();

    int filefd;

    if (pid == -1) {
        printf("\nFailed forking child..");
        return;
    } else if (pid == 0) {
        filefd = open(file, O_CREAT | O_WRONLY, 0666);
        dup2(filefd, STDOUT_FILENO);
        exitstatus = execvp(arglist[0], arglist);
//        close(filefd);
        perror("execvp failed");
        exit(1);
    } else {
        // waiting for child to terminate
        if (wait_for_child) {
            while (wait(&exitstatus) != pid) { ; }
        }
    }
}


void execArgsRedirectStdIn(char *argbuf) {
    /* redirects arguments in a file to stdin
     * works only if there is no whitespace between
     * the '<' symbol and the file name
     */

    char *cp = copyStr(argbuf);

    char **split = str_split(cp, '<');
    char *args = split[0];
    char *file = split[1];
    char **arglist = str_split(args, ' ');


    int pid, exitstatus;
    pid = fork();
    int filefd;

    if (pid == -1) {
        printf("\nFailed forking child..");
        return;
    } else if (pid == 0) {
        filefd = open(file, O_RDONLY, 0666);
        dup2(filefd, STDIN_FILENO);
        close(filefd);
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
    } else {
        // waiting for child to terminate
        while (wait(&exitstatus) != pid) { ; }
    }
}


void execArgsPiped(char *argbuf)
{
    /*
     * handles pipe between args
     **/
    char *cp = copyStr(argbuf);

    char **split = str_split(cp, '|');
    char *args = split[0];
    char *pipeargs = split[1];
    int numpipeargs = getNumArgs(pipeargs);  // will be used if there's an '&' at the end
    char **arglist = str_split(args, ' ');
    char **pipearglist = str_split(pipeargs, ' ');

    int wait_for_child = 1;
    if (argBufEndsInAmpersand(argbuf)) {
        /* This assumes there is a space between the last arg and '&' */
        wait_for_child = 0;
        pipearglist[numpipeargs - 1] = NULL;
    }

    // 0 is read end, 1 is write end
    int pipefd[2];
    int p1, p2;
    int exitstatus1, exitstatus2;

    if (pipe(pipefd) < 0) {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0) {
        printf("\nCould not fork");
        return;
    }

    if (p1 == 0) {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(1);
    } else {
        // Parent executing
        p2 = fork();

        if (p2 < 0) {
            printf("\nCould not fork");
            return;
        }

        // Child 2 executing..
        // It only needs to read at the read end
        if (p2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execvp(pipearglist[0], pipearglist);
            perror("execvp failed");
            exit(1);
        } else {
            // parent executing, waiting for two children
            close(pipefd[1]);  // close the write end of the pipe in the parent
            if (wait_for_child) {
                while (wait(&exitstatus1) != p1) { ; }
                while (wait(&exitstatus2) != p2) { ; }
            }
        }
    }
}


int checkForChar(char* buf, const char *adelim) {
    /* checks for char in a string */
    char *cp = copyStr(buf);

    char* myarr[2];
    int i;
    for (i = 0; i < 2; i++) {
        myarr[i] = strsep(&cp, adelim);
        if (myarr[i] == NULL)
            break;
    }

    if (myarr[1] == NULL)
        return 0; // returns zero if no pipe is found.
    else {
        return 1;
    }

}


int main()
{
    char inputString[MAX_LINE/2 + 1];
    char *argbuf;
    int should_run = 1;

    while (should_run) {
        // Print shell line and take input
        printf("\nosh>");
        fflush(stdout);
        fgets(inputString, sizeof(inputString), stdin);
        // Get the buffer string, which will be used by all the helper functions
        argbuf = makestring(inputString);

        // If the input string (argbuf) is empty
        int numargs = getNumArgs(argbuf);
        if (numargs == 0) {
            continue;
        }

        // Parse argbuf for special commands like 'exit' and '!!'
        // (could also call exit when 'exit' is encountered)
        char *cp = copyStr(argbuf);
        char **tokens = str_split(cp, ' ');  // the numargs check ensures there's something in this

        if (strcmp(tokens[0], "exit") == 0) {
            printf("\nGoodbye.\n");
            //        exit(0);  // could exit here too
            should_run = 0;
        } else if (strcmp(tokens[0], "!!") == 0) {
            if (prevbuffer == NULL) {
                printf("No commands in history.\n");
                continue;
            } else {
                printf("osh>%s\n", prevbuffer);
                strcpy(argbuf, prevbuffer);
            }
        }

        if (!should_run) {
            // This will cause the shell to terminate
            continue;
        }

        // Check for redirects and pipes
        if (checkForChar(argbuf, ">")) {
            // if there's stdout redirect
            execArgsRedirectStdOut(argbuf);

        } else if (checkForChar(argbuf, "<") ) {
            // if there's stdin redirect
            execArgsRedirectStdIn(argbuf);

        } else if (checkForChar(argbuf, "|")) {
            // if there's a pipe
            execArgsPiped(argbuf);

        } else {
            // if no redirects or pipes
            execArgs(argbuf);
        }

        // save the current command
        add_history(argbuf);

    }
    return 0;
}

