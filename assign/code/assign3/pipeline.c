/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

void pipeline(char *argv1[], char *argv2[], int pids[]) {
  int fds[2];
  pipe(fds);
  pids[0] = fork();
  if(pids[0] == 0)
  {
    // first child process, redirect its std output to fds[1];
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execvp(argv1[0], argv1);
    printf("Failed to execute the supplied command for the first child process.");
    exit(0);
  }
  pids[1] = fork();
  if(pids[1] == 0)
  {
    // second child process, redirect its std input to fds[0];
    close(fds[1]);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    execvp(argv2[0], argv2);
    printf("Failed to execute the supplied command for the second child process.");
    exit(0);
  }
  close(fds[1]);
  close(fds[0]);
}
