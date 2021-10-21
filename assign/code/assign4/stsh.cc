/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-parser/stsh-parse-exception.h"
#include "stsh-signal.h"
#include "stsh-job-list.h"
#include "stsh-job.h"
#include "stsh-process.h"
#include "stsh-parse-utils.h"
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>  // for fork
#include <signal.h>  // for kill
#include <sys/wait.h>
using namespace std;

static STSHJobList joblist; // the one piece of global data we need so signal handlers can access it
void continueJob(size_t job_number, STSHJobState state);
void sendToProcess(const char *const argv[], int signal);
/**
 * Function: handleBuiltin
 * -----------------------
 * Examines the leading command of the provided pipeline to see if
 * it's a shell builtin, and if so, handles and executes it.  handleBuiltin
 * returns true if the command is a builtin, and false otherwise.
 */
static const string kSupportedBuiltins[] = {"quit", "exit", "fg", "bg", "slay", "halt", "cont", "jobs"};
static const size_t kNumSupportedBuiltins = sizeof(kSupportedBuiltins)/sizeof(kSupportedBuiltins[0]);
static bool handleBuiltin(const pipeline& pipeline) {
  const string& command = pipeline.commands[0].command;
  auto iter = find(kSupportedBuiltins, kSupportedBuiltins + kNumSupportedBuiltins, command);
  if (iter == kSupportedBuiltins + kNumSupportedBuiltins) return false;
  size_t index = iter - kSupportedBuiltins;

  switch (index) {
  case 0:
  case 1: exit(0);
  case 2: continueJob(parseNumber(pipeline.commands[0].tokens[0], "Parse fg job number"), STSHJobState::kForeground); break; 
  case 3: continueJob(parseNumber(pipeline.commands[0].tokens[0], "Parse bg job number"), STSHJobState::kBackground); break;
  case 4: sendToProcess(&pipeline.commands[0].tokens[0], SIGKILL); break;
  case 5: sendToProcess(&pipeline.commands[0].tokens[0], SIGTSTP); break;
  case 6: sendToProcess(&pipeline.commands[0].tokens[0], SIGCONT); break; 
  case 7: cout << joblist; break;
  default: throw STSHException("Internal Error: Builtin command not supported."); // or not implemented yet
  }
  
  return true;
}

static void toggleSignalBlock(int signal, int how) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, signal);
  sigprocmask(how, &mask, NULL);
}

void blockSignal(int signal) {
  toggleSignalBlock(signal, SIG_BLOCK);
}
void unblockSignal(int signal) {
  toggleSignalBlock(signal, SIG_UNBLOCK);
}

void handle_SIGSTP_and_SIGINT(int sig_num)
{
  if(joblist.hasForegroundJob())
  {
    STSHJob& job = joblist.getForegroundJob();
    pid_t goup_pid = job.getGroupID();
    killpg(goup_pid, sig_num);
    std::cout << ((sig_num == SIGINT) ? "SIGINT came" : "SIGSTP came") << std::endl;
  }
}

void handle_SIGCHLD(int unused)
{
  (void)unused;
  while(true)
  {
    int status;
    std::cout << "before wait" << std::endl;
    pid_t pid = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED);
    std::cout << "after wait, pid==" << pid << std::endl;
    if(pid <= 0) break;
    STSHJob& job = joblist.getJobWithProcess(pid);
    STSHProcess &process = job.getProcess(pid);
    if(WIFEXITED(status)) 
    {
      std::cout << "EXITED" << std::endl;
      process.setState(STSHProcessState::kTerminated);
    }
    else if(WIFSIGNALED(status))
    {
      std::cout << "SIGNALED exit" << std::endl;
      process.setState(STSHProcessState::kTerminated);
    }else if(WIFSTOPPED(status))
    {
      std::cout << "STOPPED" << std::endl;
      process.setState(STSHProcessState::kStopped);
    }else if(WIFCONTINUED(status))
    {
      std::cout << "CONTINUED" << std::endl;
      process.setState(STSHProcessState::kRunning);
    }
    joblist.synchronize(job);
  }
}

/**
 * Function: installSignalHandlers
 * -------------------------------
 * Installs user-defined signals handlers for four signals
 * (once you've implemented signal handlers for SIGCHLD, 
 * SIGINT, and SIGTSTP, you'll add more installSignalHandler calls) and 
 * ignores two others.
 */
static void installSignalHandlers() {
  installSignalHandler(SIGQUIT, [](int sig) { exit(0); });
  installSignalHandler(SIGTTIN, SIG_IGN);
  installSignalHandler(SIGTTOU, SIG_IGN);
  installSignalHandler(SIGCHLD, handle_SIGCHLD);
  installSignalHandler(SIGTSTP, handle_SIGSTP_and_SIGINT);
  installSignalHandler(SIGINT, handle_SIGSTP_and_SIGINT);
}

void waitForegroundJob()
{
  sigset_t empty;
  sigemptyset(&empty);
  while(joblist.hasForegroundJob())
  {
    sigsuspend(&empty);
  }
  // stsh takes back control over terminal
  if(tcsetpgrp(STDIN_FILENO, getpgid(0)) == -1)
  {
    throw STSHException("Shell failed to retrieve the terminal.");
  }
  cout << "Joblist when there are no more foreground jobs:" << endl << joblist;
}

void continueJob(size_t job_number, STSHJobState state)
{
  blockSignal(SIGCHLD);
  blockSignal(SIGTSTP);
  blockSignal(SIGINT);

  if (!joblist.containsJob(job_number)) throw STSHException("Undefined job number " + to_string(static_cast<int>(job_number)));
  STSHJob &job = joblist.getJob(job_number);
  job.setState(state);  
  pid_t group_id = job.getGroupID();
  if(state == STSHJobState::kForeground)
  {
    if(tcsetpgrp(STDIN_FILENO, group_id) == -1)
    {
      throw STSHException("Faild to give terminal control to the process returning from background/stopped state");
    }
  }
  killpg(group_id, SIGCONT);
  
  if(state == STSHJobState::kForeground)
  {
    waitForegroundJob();
  }

  unblockSignal(SIGCHLD);
  unblockSignal(SIGTSTP);
  unblockSignal(SIGINT);
}

void sendToProcess(const char *const argv[], int signal)
{
  if((argv[0] == NULL) || (argv[1] != NULL && argv[2] != NULL))
  {
    throw STSHException("Invalid number of arguments for slay/halt/cont.");
  }
  bool pid_args = true;
  size_t first = parseNumber(argv[0], "Parse argument for slay/halt/cont");
  size_t second;
  if(argv[1] != NULL)
  {
    pid_args = false;
    second = parseNumber(argv[1], "Parse argument for slay/halt/cont");
  }
  if(pid_args)
  {
    if(!joblist.containsProcess(static_cast<int>(first)))
    {
      throw STSHException("Process with the specified pid does not exist.");
    }
    kill(static_cast<int>(first), signal);
  }else
  {
    if(joblist.containsJob(first))
    {
      STSHJob &job = joblist.getJob(first);
      const std::vector<STSHProcess> &processes = job.getProcesses();
      if(second < processes.size())
      {
        pid_t pid = processes[second].getID();
        kill(pid, signal);
      }else
      {
        STSHException("Specified job process index is invalid");
      }
    }else
    {
      throw STSHException("Job with the number " + std::to_string(first) + " doesn't exist");
    }
  }
}

void printJobSummary(const STSHJob &job)
{
  std::cout << '[' << job.getNum() << "] ";
  for(const STSHProcess& process: job.getProcesses())
  {
    std::cout << process.getID() << ' ';
  }
  std::cout << '\n';
}

/**
 * Function: createJob
 * -------------------
 * Creates a new job on behalf of the provided pipeline.
 */
static void createJob(const pipeline& p) {
  const command& cmnd = p.commands[0];
  // create arguments for process on stack
  char array_chars[kMaxArguments + 2][kMaxCommandLength + 1] = {'\0'};
  char *argv[kMaxArguments + 2] = {NULL};
  strcpy(array_chars[0], cmnd.command);
  argv[0] = array_chars[0];
  for(size_t i = 1; (i < kMaxArguments + 2) && (cmnd.tokens[i - 1] != NULL); i++)
  {
    strcpy(array_chars[i], cmnd.tokens[i - 1]);
    argv[i] = array_chars[i];
  }
  blockSignal(SIGCHLD);
  blockSignal(SIGTSTP);
  blockSignal(SIGINT);
  STSHJob& job = joblist.addJob(STSHJobState::kForeground);
  if(p.background)
  {
    job.setState(STSHJobState::kBackground);
  }
  // get process group ID, if there are no processes it returns 0, which is intended.
  pid_t pgid = job.getGroupID();
  pid_t pid = fork();
  if(pid == 0)
  {
    setpgid(0, pgid);
    if(pgid == 0 && job.getState() == STSHJobState::kForeground)
    {
      if(tcsetpgrp(STDIN_FILENO, getpgid(0)) == -1)
      {
        throw STSHException("Faild to give terminal to the foreground job");
      }
    }
    installSignalHandler(SIGCHLD, SIG_DFL);
    installSignalHandler(SIGTSTP, SIG_DFL);
    installSignalHandler(SIGINT, SIG_DFL);
    unblockSignal(SIGCHLD);
    unblockSignal(SIGTSTP);
    unblockSignal(SIGINT);

    execvp(argv[0], argv);
    // if we step in this line, something went wrong, execvp should never return.
    throw STSHException("Failed to invoke /bin/sh to execute the supplied command.");
  }
  setpgid(pid, pgid);
  job.addProcess(STSHProcess(pid, cmnd, kRunning));
  // cout << "Joblist after adding process:" << endl << joblist;
  if(job.getState() == STSHJobState::kForeground)
  {
    waitForegroundJob();
  }else
  {
    printJobSummary(job);
  }

  unblockSignal(SIGCHLD);
  unblockSignal(SIGTSTP);
  unblockSignal(SIGINT);
}

/**
 * Function: main
 * --------------
 * Defines the entry point for a process running stsh.
 * The main function is little more than a read-eval-print
 * loop (i.e. a repl).  
 */
int main(int argc, char *argv[]) {
  pid_t stshpid = getpid();
  installSignalHandlers();
  rlinit(argc, argv);  // configures stsh-readline library so readline works properly
  while (true) {
    string line;
    if (!readline(line)) break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      bool builtin = handleBuiltin(p);
      if (!builtin) createJob(p); // createJob is initially defined as a wrapper around cout << p;
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }

  return 0;
}
