/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
#include "errno.h"
using namespace std;

void pipe_wrapper(int *fds)
{
	int status = pipe(fds);
	if(status == -1) throw SubprocessException("SubprocessException: could not create a pipe. Error number " + to_string(errno));
}

void close_wrapper(int file_descriptor)
{
	int status = close(file_descriptor);
	if(status == -1) throw SubprocessException("SubprocessException: close a file. Error number " + to_string(errno));
}

void dup2_wrapper(int fd1, int fd2)
{
	int status = dup2(fd1, fd2);
	if(status == -1) throw SubprocessException("SubprocessException: error in dup2 function. Error number " + to_string(errno));
}

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) {
	int fds_supply[2];
	int fds_ingest[2];
	
	if(supplyChildInput) pipe_wrapper(fds_supply);
	if(ingestChildOutput) pipe_wrapper(fds_ingest);
	
	subprocess_t process{};
	
	process.pid = fork();
	if(process.pid == -1) throw SubprocessException("SubprocessException: could not create a child process. Error number " + to_string(errno));
	else if(process.pid == 0)
	{
		// handle supply
		if(supplyChildInput)
		{
			close_wrapper(fds_supply[1]);
			dup2_wrapper(fds_supply[0], STDIN_FILENO);
			close_wrapper(fds_supply[0]);
		}

		//handle ingest
		if(ingestChildOutput)
		{
			close_wrapper(fds_ingest[0]);
			dup2_wrapper(fds_ingest[1], STDOUT_FILENO);
			close_wrapper(fds_ingest[1]);
		}
		
		execvp(argv[0], argv);
		throw SubprocessException("SubprocessException: error calling command with execvp");
	}

	// parent fills the process structure
	if(supplyChildInput)
	{
		close_wrapper(fds_supply[0]);
		process.supplyfd = fds_supply[1];
	}else
	{
		process.supplyfd = kNotInUse;
	}

	if(ingestChildOutput)
	{
		close_wrapper(fds_ingest[1]);
		process.ingestfd = fds_ingest[0];
	}else
	{
		process.ingestfd = kNotInUse;
	}
	
	return process;
}
