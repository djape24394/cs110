/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
using namespace std;

ThreadPool::ThreadPool(size_t numThreads) : wts(numThreads), nofAvailableWorkers(numThreads), availableWorkers(numThreads, true)
{
    for(size_t i = 0; i < numThreads; i++)
    {
        wt_thunks.emplace_back(nullptr);
        wt_semaphores.emplace_back(make_unique<semaphore>(0));
    }

    // launch dispatcher
    dt = thread([this]()
                { dispatcher(); });
    
    // launch worker
    for (size_t workerID = 0; workerID < numThreads; workerID++)
    {
        wts[workerID] = thread([this, workerID]()
                               { worker(workerID); });
    }
}
void ThreadPool::schedule(const Thunk &thunk) 
{
    lock_guard<mutex> lg_wait(waitLock);
    lock_guard<mutex> lg(jobsLock);
    jobs.push(thunk);
    cvJobs.notify_all();
}

void ThreadPool::wait() 
{   
    lock_guard<mutex> lg_wait(waitLock);
    
    {
        lock_guard<mutex> lg(jobsLock);
        cvJobs.wait(jobsLock, [this](){
            return jobs.empty();
        });
    }
    
    {
        lock_guard<mutex> lg(availableWorkersLock);
        cvWorkers.wait(availableWorkersLock, [this](){
            return nofAvailableWorkers == availableWorkers.size();
        });
    }
}

ThreadPool::~ThreadPool() 
{
    wait();
    exitFlag = true;
    cvJobs.notify_all();
    for(auto& smphr: wt_semaphores)
    {
        smphr->signal();
    }
    dt.join();
    for(auto& thrd: wts) thrd.join();
}

void ThreadPool::dispatcher() 
{
    while(true)
    {
        {
            lock_guard<mutex> lg(jobsLock);
            cvJobs.wait(jobsLock, [this]()
            {
                return !jobs.empty() || exitFlag;
            });
            if(exitFlag) break;
        }

        size_t worker_id = 0;
        {
            lock_guard<mutex> lg(availableWorkersLock);
            // TODO: consider using only one semaphore for this
            cvWorkers.wait(availableWorkersLock, [this](){
                return nofAvailableWorkers > 0;
            });
            for(size_t i = 0; i < availableWorkers.size(); i++)
            {
                if(availableWorkers[i])
                {
                    worker_id = i;
                    availableWorkers[i] = false;
                    nofAvailableWorkers--;
                    break;
                }
            }
        }

        {
            lock_guard<mutex> lg(jobsLock);
            Thunk thunk = jobs.front();
            jobs.pop();
            wt_thunks[worker_id] = thunk;
            wt_semaphores[worker_id]->signal();
            cvJobs.notify_all();
        }
    }
}

void ThreadPool::worker(size_t workerID) 
{
    while(true)
    {
        wt_semaphores[workerID]->wait();
        if(exitFlag) break;
        wt_thunks[workerID]();

        {
            lock_guard<mutex> lg(availableWorkersLock);
            nofAvailableWorkers++;
            availableWorkers[workerID] = true;
            cvWorkers.notify_all();
        }
    }
}
