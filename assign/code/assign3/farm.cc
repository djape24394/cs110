#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"

using namespace std;

struct worker {
  worker() {}
  worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
  subprocess_t sp;
  bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static void markWorkersAsAvailable(int sig) {
  while (true) {
    pid_t pid = waitpid(-1, NULL, WNOHANG | WUNTRACED);
    if (pid <= 0) break;
    for(worker& w: workers)
    {
      if(w.sp.pid == pid)
      {
        w.available = true;
        numWorkersAvailable++;
        break;
      }
    }
  }
}

static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};

static void spawnAllWorkers() {
  cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
  for (size_t i = 0; i < kNumCPUs; i++) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(i, &set);
    workers[i] = worker((char **)kWorkerArguments);
    if(sched_setaffinity(workers[i].sp.pid, sizeof(set), &set) == -1)
    {
      cout << "ERROR setting affinity for process " << workers[i].sp.pid << " to CPU " << i << endl;
      exit(-1);
    }
    cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
  }
}

static size_t getAvailableWorker() {
  sigset_t mask, oldmask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  while (numWorkersAvailable == 0)
  {
    sigsuspend(&oldmask);
  }
  size_t idx = kNumCPUs + 1;
  for(size_t i = 0; i < kNumCPUs; i++)
  {
    if(workers[i].available)
    {
      idx = i;
      break;
    }
  }
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  return idx;
}

static void broadcastNumbersToWorkers() {
  while (true) {
    string line;
    getline(cin, line);
    if (cin.fail()) break;
    size_t endpos;
    long long num =  stoll(line, &endpos);
    if (endpos != line.size()) break;
    // you shouldn't need all that many lines of additional code
    size_t worker_id = getAvailableWorker();
    if(worker_id >= kNumCPUs || workers[worker_id].available == false)
    {
      cout << "Something went wrong while waiting for worker!!\n";
      break;
    }
    workers[worker_id].available = false;
    numWorkersAvailable--;
    dprintf(workers[worker_id].sp.supplyfd, "%lld\n", num);
    kill(workers[worker_id].sp.pid, SIGCONT);
  }
}

static void waitForAllWorkers() 
{
  sigset_t mask, oldmask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  while (numWorkersAvailable < kNumCPUs)
  {
    sigsuspend(&oldmask);
  }
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

static void closeAllWorkers() 
{
  signal(SIGCHLD, SIG_DFL);
  for(worker& w: workers)
  {
    close(w.sp.supplyfd);
    kill(w.sp.pid, SIGCONT);
  }

	// check all workers come back
	for(size_t i = 0; i < kNumCPUs; i++) {
		while(true) {
			int status;
			waitpid(workers[i].sp.pid, &status, 0);
			if(WIFEXITED(status)) {
				break;
			}
		}
	}

}

int main(int argc, char *argv[]) {
  signal(SIGCHLD, markWorkersAsAvailable);
  spawnAllWorkers();
  broadcastNumbersToWorkers();
  waitForAllWorkers();
  closeAllWorkers();
  return 0;
}
