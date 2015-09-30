/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive:
 $> tar cvf lab1.tgz lab1/
 *
 * All the best
 */


#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>
#include <unistd.h>

/*default non-built in library*/
#include "parse.h"

/*general functions*/
void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void CountCommand(Command *);
void stripwhite(char *);

/*main bash for single commands*/
void simpleBash(Command *cmd);

/*signal handler*/
void cleanChild(int signal);
void ctrlC(int signal);

/*specific executions*/
void execBackground(int cPid, Command *cmdin);
void execNormal(int cPid, Command *cmdin);

/*pipe*/
void loopPipe(char ***cmdList, Command *cmd);
void initPipe(Command *cmdin);

/*dynamic prompt*/
void charCount(char *prompt);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*global vars*/
sigjmp_buf ctrlc_jump;
pid_t globalPid;
int numOfCmd = 0;
int backgroundFlag = 0;
char promptLine[1024];
char finalPrompt[1024];
/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */

#define CHECK(x) if(!(x)) { perror(#x " failed"); abort(); /* or whatever */ }

int main(int argc, char **argv)
{
    Command cmd;
    int n;
    
    /*get the current directory*/
    memset(promptLine, '\0', 1024 * sizeof(char));
    getcwd(promptLine, sizeof(promptLine));
    charCount(promptLine);
    
    /*clear screen*/
    system("clear");
    
    /*place for longjump*/
    while ( sigsetjmp( ctrlc_jump, 1 ) != 0 );
    
    while (!done)
    {
        /*reset the numOfCmd globalvar*/
        numOfCmd = 0;
        
        char *line;
        line = readline(finalPrompt);
        
        if (!line)
        {
            /* Encountered EOF at top level */
            done = 1;
        }
        else
        {
            /*
             * Remove leading and trailing whitespace from the line6
             * Then, if there is anything left, add it to the history list
             * and execute it.
             */
            stripwhite(line);
            
            if(*line)
            {
                add_history(line);
                
                /* execute it */
                n = parse(line, &cmd);
                CountCommand(&cmd);
                
                /*once we got the parsed Command Switcher, initialized global var*/
                backgroundFlag = cmd.bakground;
                
                /*check if the input is either 'exit', 'cd', 'sleep' etc*/
                if ((!strcmp((*cmd.pgm->pgmlist), "exit")))
                {
                    /*go to exit*/
                    return 0;
                }
                
                else if ((!strcmp((*cmd.pgm->pgmlist), "cd")))
                {
                    /*change directory*/
                    if (chdir(*(cmd.pgm->pgmlist+1)) == -1)
                        perror("error");
                    
                    memset(promptLine, '\0', 1024 * sizeof(char));
                    getcwd(promptLine, sizeof(promptLine));
                    charCount(promptLine);
                }
                
                else
                {
                    /*check if there is a next or not*/
                    if (!cmd.pgm->next)
                    /*regular bash function*/
                        simpleBash(&cmd);
                    else
                    {
                        /*we're doing the pipe-specific functions*/
                        initPipe(&cmd);
                    }
                    
                }
            }
        }
        
        if(line)
            free(line);
        
    }
    
    return 0;
}

/*
 * Name: charCount
 *
 * Description: parsing last working directory from whole directories
 *  need to use two global char variables
 *      - finalPrompt   (output)
 *      - prompt        (input)
 */
void charCount(char *prompt)
{
    int i=1023, temp;
    int countOfChar = 0;
    
    char padding1[] = "amirjanu:";
    char padding2[] = "$ ";
    
    while (i!=0)
    {
        if (prompt[i] != '\0')
        {
            countOfChar++;
            
            if (prompt[i] == '/')
                break;
        }
        
        i--;
    }
    
    memcpy(&finalPrompt[0], padding1, sizeof(padding1));
    memcpy(&finalPrompt[sizeof(padding1)-1], &prompt[i], countOfChar * sizeof(char));
    memcpy(&finalPrompt[sizeof(padding1)-1 + countOfChar], padding2, sizeof(padding2));
}

/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void PrintCommand (int n, Command *cmd)
{
    printf("Parse returned %d:\n", n);
    printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
    printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
    printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
    PrintPgm(cmd->pgm);
}

/*
 * Name: CountCommand
 *
 * Description: Function to count number of commands inside lists
 */
void CountCommand (Command *cmd)
{
    PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void PrintPgm (Pgm *p)
{
    if (p == NULL)
    {
        return;
    }
    else
    {
        char **pl = p->pgmlist;
        
        /* The list is in reversed order so print
         * it reversed to get right
         */
        PrintPgm(p->next);
        
        /*
         printf("command: %s\n", *pl);
         
         printf("    [");
         while (*pl)
         {
         printf("%s ", *pl++);
         }
         printf("]\n");
         */
    }
    
    /*count the number of list*/
    numOfCmd++;
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void stripwhite (char *string)
{
    register int i = 0;
    
    while (whitespace( string[i] ))
    {
        i++;
    }
    
    if (i)
    {
        strcpy (string, string + i);
    }
    
    i = strlen( string ) - 1;
    
    while (i> 0 && whitespace (string[i]))
    {
        i--;
    }
    
    string [++i] = '\0';
}

/*
 * Name: cleanChild
 *
 * Description: Child handler when the child is exiting
 */
void cleanChild(int signal)
{
    /* remove child process */
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        
    }
}

/*
 * Name: ctrlC
 *
 * Description: Handler for Interrupt (CTRL+C)
 */
void ctrlC(int signal)
{
    /*Interrupt appears before fork() process*/
    if ((globalPid == 0) && (!backgroundFlag))
    {
        printf("\n");
        
        /*jump to start of while loop*/
        siglongjmp(ctrlc_jump, 1);
    }
    /*if we have a child*/
    else
    {
        /*kill the parent and it's group*/
        //killpg(globalPid, SIGTERM);
        printf("\n");
        
        /*jump to start of while loop*/
        siglongjmp(ctrlc_jump, 1);
    }
}

/*
 * Name: simpleBash
 *
 * Description: this is the bash operation for simple command and every parameters
 */
void simpleBash(Command *cmd)
{
    /*pid_t childPid;*/
    pid_t childPid;
    pid_t endPid;
    int status;
    int fd;
    
    /*handler for Interrupt (CTRL+C) & Zombie (Child Termination after Parent terminated)*/
    signal(SIGINT, ctrlC);
    signal(SIGCHLD, cleanChild);
    
    /*create a child for single command*/
    if ((childPid = fork()) == -1)
        perror("error on fork\n");
    
    /*copy childPid to global pid*/
    globalPid = childPid;
    
    /*if there is '&' background flag*/
    if (cmd->bakground != (int)NULL)
        execBackground(childPid, cmd);
    /*if there isn't '&' background flag*/
    else
        execNormal(childPid, cmd);
    
}

/*
 * Name: execNormal
 *
 * Description: normal shell execution (single commands)
 *
 * INPUT argument:
 *  cPid:       child Pid
 *  cmdin:      Command switcher
 */
void execNormal(int cPid, Command *cmdin)
{
    int fd;
    int saveInput;
    
    /*CHILD portion*/
    if (cPid == 0)
    {
        /*write to file flag ON*/
        if (cmdin->rstdout != NULL)
        {
            printf("child is printing a file...\n");
            /*open file descriptor and match it with the filename*/
            fd = open(cmdin->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            
            /*error handler*/
            if (fd == -1)
            {
                perror("error opening file. exiting now...\n");
                exit(EXIT_FAILURE);
            }
            
            /*duplicate the file descriptor*/
            dup2(fd, 1);
        }
        
        /*reading a file flag ON*/
        if (cmdin->rstdin != NULL)
        {
            printf("child is reading a file...\n");
            /*open file descriptor and match it with the filename*/
            fd = open(cmdin->rstdin, O_RDONLY, 0600);
            if (fd == -1)
            {
                perror("error opening file. exiting now...\n");
                exit(EXIT_FAILURE);
            }
            
            saveInput = dup(fileno(stdin));
            
            dup2(fd, fileno(stdin));
        }
        
        /*exec the command*/
        if (execvp( *cmdin->pgm->pgmlist,/*pointer to main command*/
                   cmdin->pgm->pgmlist) == -1)
        {
            printf("command '%s' not found\n", (*cmdin->pgm->pgmlist));
            exit(EXIT_FAILURE);
        }
        
        /*write to file*/
        if (cmdin->rstdout != NULL)
            close(fd);
        
        /*clear, flush and close*/
        if (cmdin->rstdin != NULL)
        {
            fflush(stdin);
            close(fd);
            
            dup2(saveInput, fileno(stdin));
            close(saveInput);
        }
    }
    else if (cPid == -1)
    {
        fprintf(stderr, "fork is failed\n");
        exit(EXIT_FAILURE);
    }
    /*PARENT portion*/
    else
    {
        wait(NULL);
    }
}

/*
 * Name: execBackground
 *
 * Description: background process only shell execution (single commands)
 *
 * INPUT Argument:
 *  cPid:       child Pid
 *  cmdin:      Command switcher
 */
void execBackground(int cPid, Command *cmdin)
{
    int fd;
    int saveInput;
    int status;
    
    /*CHILD portion*/
    if (cPid == 0)
    {
        setpgid(0, 0);
        
        /*write to file*/
        if (cmdin->rstdout != NULL)
        {
            printf("child is printing a file...\n");
            
            /*open a file descriptor which links to the created file*/
            fd = open(cmdin->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd == -1)
            {
                perror("error opening file. exiting now...\n");
                exit(EXIT_FAILURE);
            }
            
            /*duplicate the file descriptor to the writing gate*/
            dup2(fd, 1);
        }
        
        /*reading a file flag ON*/
        if (cmdin->rstdin != NULL)
        {
            printf("child is reading a file...\n");
            /*open file descriptor and match it with the filename*/
            fd = open(cmdin->rstdin, O_RDONLY, 0600);
            if (fd == -1)
            {
                perror("error opening file. exiting now...\n");
                exit(EXIT_FAILURE);
            }
            
            saveInput = dup(fileno(stdin));
            
            dup2(fd, fileno(stdin));
        }
        
        if (execvp( *cmdin->pgm->pgmlist,/*pointer to main command*/
                   cmdin->pgm->pgmlist) == -1)
        {
            printf("command '%s' not found\n", (*cmdin->pgm->pgmlist));
            exit(EXIT_FAILURE);
        }
        
        
        /*write to file*/
        if (cmdin->rstdout != NULL)
            close(fd);
        
        /*clear, flush and close*/
        if (cmdin->rstdin != NULL)
        {
            fflush(stdin);
            close(fd);
            
            dup2(saveInput, fileno(stdin));
            close(saveInput);
        }
        
        abort();
    }
    /*error handler for fork*/
    else if (cPid == -1)
    {
        fprintf(stderr, "fork is failed\n");
        exit(EXIT_FAILURE);
    }
    /*PARENT portion*/
    else
    {
        /*return if child is still running in background*/
        setpgid(getpid(), getpid());
        waitpid(0, NULL, WNOHANG);
    }
}

/*
 * Name: loopPipe
 *
 * Description: main pipe-specific shell execution (multiple commands)
 *
 * INPUT Argument:
 *  cmdList:    List of commands in (pointer to 2D arrays)
 *  cmdin:      Command switcher
 */
void loopPipe(char ***cmdList, Command *cmd)
{
    int   p[2];
    pid_t pid;
    int   fd_in = 0;
    int   saveInput;
    
    /*iterate through the ordered command lists*/
    while (*cmdList != NULL)
    {
        /*create a pipe for every two lists*/
        pipe(p);
        
        /*check for fork error*/
        if ((pid = fork()) == -1)
        {
            exit(EXIT_FAILURE);
        }
        /*CHILD portion*/
        else if (pid == 0)
        {
            /*check if there is a reading to a file flag*/
            if (cmd->rstdin != NULL)
            {
                /*open file descriptor and match it with the filename*/
                fd_in = open(cmd->rstdin, O_RDONLY, 0600);
                if (fd_in == -1)
                {
                    perror("error opening file. exiting now...\n");
                    exit(EXIT_FAILURE);
                }
                
                saveInput = dup(fileno(stdin));
                
                dup2(fd_in, fileno(stdin));
            }
            else
            /*change the input according to the old one*/
                dup2(fd_in, 0);
            
            /*check if the next list is there or not*/
            if (*(cmdList + 1) != NULL)
            /*if yes, duplicate correspond pipe for writing*/
                dup2(p[1], 1);
            
            /*check if we reach the end of list and there is a writing to a file flag*/
            else if ((*(cmdList + 1) == NULL) && (cmd->rstdout != NULL))
            {
                /*now open pipe for writing into created file*/
                p[1] = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (p[1] == -1)
                {
                    perror("error opening file. exiting now...\n");
                    exit(EXIT_FAILURE);
                }
                dup2(p[1], 1);
            }
            
            /*close unnecessary older pipe*/
            close(p[0]);
            
            /*run the command*/
            if(execvp((*cmdList)[0], *cmdList) == -1)
            {
                printf("command '%s' not found\n", **cmdList);
                exit(EXIT_FAILURE);
            }
            
            /*clear, flush and close*/
            if (cmd->rstdin != NULL)
            {
                fflush(stdin);
                close(fd_in);
                
                dup2(saveInput, fileno(stdin));
                close(saveInput);
            }
            
            exit(EXIT_FAILURE);
        }
        else
        {
            /*PARENT portion. it checks whether we already in the last list and checking for background flag*/
            if ((cmd->bakground != (int)NULL) && (*(cmdList+1) == NULL))
            {
                close(p[1]);
                
                /*save the input for the next command*/
                fd_in = p[0];
                cmdList++;
                
                /*if there is running in background flag, PARENT doesn't need to wait*/
                return;
            }
            else
            {
                /*let PARENT waits for the last child running last list of command finished*/
                wait(NULL);
                close(p[1]);
                
                /*save the input for the next command*/
                fd_in = p[0];
                cmdList++;
            }
        }
    }
}

/*
 * Name: initPipe
 *
 * Description: wrapper for the pipe. consists of preparation of the input lists
 *
 * INPUT Argument:
 *  cmdin:      Command switcher
 */
void initPipe(Command *cmdin)
{
    int len = 0;
    
    /*duplicate the pgmlist, it contains next and pgmlist*/
    Pgm *head;
    
    /*allocate a pointer of pointer of pointer*/
    char ***listOfInput = malloc((numOfCmd+1)*sizeof(char**));
    
    /*copy the pgm*/
    head = cmdin->pgm;
    
    /*start reorder-filling for our listOfInput command array*/
    int pointer = numOfCmd - 1;
    while(head != NULL)
    {
        listOfInput[pointer] = head->pgmlist;
        
        /*the iteration*/
        pointer--;
        head = head->next;
    }
    /*don't forget to padd the last char with NULL for execvp*/
    listOfInput[numOfCmd] = NULL;
    
    /*call the piping routine*/
    loopPipe(listOfInput, cmdin);
    
    /*free the array*/
    free(listOfInput);
}