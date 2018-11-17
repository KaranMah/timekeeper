/************************************************************
* Filename: timekeeper_3035346025.c
* Student name: KARAN MAHAJAN
* Student no.: 3035346025
* Development platform: X2Go
* Compilation: gcc timekeeper_3035346025.c –o timekeeper
* Remark: Complete all 3 stages
*************************************************************/


#include<stdio.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<time.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<ctype.h>
#include<stdlib.h>


/*function to get values from status file*/
void get_status(long int pid, int *numContext) {

    char path[40], line[100];
    FILE *statusf;
    char read1[20000], read2[20];
    snprintf(path, 40, "/proc/%ld/status", pid);
    statusf = fopen(path, "r");

    if (!statusf) {
        printf("error\n");
        return;
    }

    int voluntary, nonvoluntary;
    /*read file for data*/
    while (fscanf(statusf, "%s%[^\n]", read1, read2) != EOF) {
        if (strcmp(read1, "voluntary_ctxt_switches:") == 0) {
            voluntary = atoi(read2);
        } else if (strcmp(read1, "nonvoluntary_ctxt_switches:") == 0) {
            nonvoluntary = atoi(read2);
        }
    }
    *numContext = (nonvoluntary + voluntary);

    fclose(statusf);

}


/*function to get values from stat file*/
void get_stat(long int pid, float *utime, float *stime) {

    char path[40], line[100];
    FILE *statf;
    char Sutime[20], Sstime[20], read[20];
    int i = 0;

    snprintf(path, 40, "/proc/%ld/stat", pid);
    statf = fopen(path, "rb");

    if (!statf) {
        printf("error\n");
        return;
    }

    /*read data from file*/
    while (fscanf(statf, "%s", read) != EOF) {
        i++;
        if (i == 14) {
            strcpy(Sutime, read);
            *utime = strtof(Sutime, NULL);
        } else if (i == 15) {
            strcpy(Sstime, read);
            *stime = strtof(Sstime, NULL);
        }
    }

    /*convert time into sec*/
    *utime = (*utime) / sysconf(_SC_CLK_TCK);
    *stime = (*stime) / sysconf(_SC_CLK_TCK);

    fclose(statf);
}


/*Display whether successfull or not and display signal*/
void displaySignal(int *status, char *argv, int *pid) {
    if (WIFEXITED(*status) && !WEXITSTATUS(*status)) {
        /* the program terminated normally and executed successfully */
        printf("The command \"%s\" terminated with returned status code = %d \n", argv, WEXITSTATUS(*status));
    }
        /* there was an issue with exec */
    else if (WIFEXITED(*status) && WEXITSTATUS(*status) && WEXITSTATUS(*status) == 127) {
        printf("timekeeper experienced an error in starting the command: \"%s\" : %d\n", argv, *pid);
    }
        /* the program didn't terminate normally */
    else {
        if (WIFSIGNALED(*status))
            printf("The command \"%s\" is interrupted by the signal number = %d (%s)\n", argv, WTERMSIG(*status),
                   sys_siglist[WTERMSIG(*status)]);
    }
}

// main function
int main(int argc, char *argv[]) {
    clock_t t0;
    struct sigaction sa;
    id_t pid;
    int *pipeloc;
    int i, numpipe = 0;

    /*count number of pipes(numpipe) and mark location of ith pipe in pipeloc[i]*/
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "!") == 0) {
            pipeloc[numpipe] = i;
            numpipe++;
        }
    }

// check for number of inputs >1
    if (argc != 1) {
        t0 = clock();
        /*create child process to handle the input commands*/

        pid = fork();

        /* child process */
        if (pid == 0) {

            int fd[100][2], cid1, context, status;
            int start = 1;
            int end = pipeloc[0] - 1;
            char **string;

            /* if there are pipes execute this block */
            if (numpipe) {
                /* set up pipes */
                for (i = 0; i < numpipe; i++) {
                    pipe(fd[i]);
                }
                /* loop through all sets of commands*/
                for (i = 0; i <= numpipe; i++) {

                    /* set start and end values to extract data from argv
                     * start value is the location after pipe "!" and end is location before pipe*/
                    if (0 != i && i != numpipe) {
                        start = end + 2;
                        end = pipeloc[i] - 1;
                    } else if (i == numpipe) {
                        start = end + 2;
                        end = argc;
                    }
                    /* create child to execute each command */
                    t0 = clock();
                    cid1 = fork();

                    //child process
                    if (!cid1) {        /* create a subarray of argv that has the command and arguments to be execute by the child */
                        int j;
                        string = malloc(sizeof(char) * (end - start + 1));
                        for (j = 0; j < end - start + 1; j++) {
                            string[j] = argv[start + j];
                        }

                        printf("Process with id: %d created for the command: %s\n", (int) getpid(), argv[start]);

                        /*set up read and write ends of pipe*/
                        if (i != 0) {
                            dup2(fd[i - 1][0], 0);
                        }
                        if (i != numpipe) {
                            dup2(fd[i][1], 1);
                        }

                        /* close all pipes in child process */
                        for (j = 0; j < numpipe; j++) {
                            close(fd[j][0]);
                            close(fd[j][1]);
                        }
                        /* set last entry in string array NULL */
                        if (i != numpipe) string[end + 1] = (char *) NULL;

                        /* exec command */
                        execvp(string[0], string);
                        /* exit if error in exec */
                        _exit(127);
                    }


                    //parent process in fork
                    else {

                        /* close the open pipes*/
                        if (i != 0) {
                            close(fd[i - 1][0]);
                        }
                        if (i != numpipe) {
                            close(fd[i][1]);
                        }

                        /*display resources used*/
                        float utime, stime;
                        siginfo_t *info;

                        //create zombie child
                        waitid(P_PID, cid1, info, WEXITED | WSTOPPED | WNOWAIT);
                        clock_t t1 = clock();

                        //get resources data
                        get_status(cid1, &context);
                        get_stat(cid1, &utime, &stime);

                        //clear zombie
                        if (waitpid(cid1, &status, 0) > 0) {
                            displaySignal(&status, argv[start], &cid1);
                        } else {
                            printf("Error in waitpid\n");
                        }
                        /* print resource usage */
                        printf("real: %.2f s, user: %.2lf s, system: %.2f s, context switch: %d\n\n", (double) (t1 - t0) / sysconf(_SC_CLK_TCK), utime, stime, context);

                    }

                }
                /* close all pipes */
                for (i = 0; i < numpipe; i++) {
                    close(fd[i][0]);
                    close(fd[i][1]);
                }

            }
            //if only one set of commands and arguments
            else {
                printf("Process with id: %d created for the command: %s\n", (int) getpid(), argv[1]);
                execvp(argv[1], argv + 1);
            }
            _exit(127);
        }


        //timekeeper process
        else {
            signal(SIGINT, SIG_IGN);
            //ignore signal- SIGINT

            /* get and display resources used */
            int context;
            float utime, stime;

            siginfo_t *info;
            int status;

            if (numpipe == 0) {
                //wait for child to terminate
                waitid(P_PID, pid, info, WEXITED | WSTOPPED | WNOWAIT);
                clock_t t1 = clock();

                get_status(pid, &context);
                get_stat(pid, &utime, &stime);

                //clear zombie
                if (waitpid(pid, &status, 0) > 0) {
                    displaySignal(&status, argv[1], &pid);
                } else {
                    printf("Error in waitpid\n");
                }
                /* print resource usage */
                printf("real: %.2f s, user: %.2lf s, system: %.2f s, context switch: %d\n", (double) (t1 - t0) / sysconf(_SC_CLK_TCK), utime, stime, context);
            }
            waitpid(pid, &status, 0);
        }
    }
    return 0;
}
