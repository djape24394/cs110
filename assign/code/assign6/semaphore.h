#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <boost/thread/thread.hpp>

enum signal_condition
{
    on_thread_exit,
};

class semaphore
{
    int value;
    std::mutex m;
    std::condition_variable_any cv;

    semaphore(const semaphore &) = delete;
    const semaphore &operator=(const semaphore &) const = delete;

public:
    semaphore(int initialCount)
    {
        value = initialCount;
    }

    void wait()
    {
        std::lock_guard<std::mutex> lg(m);
        cv.wait(m, [this]
                { return value > 0; });
        value--;
    }

    void signal()
    {
        std::lock_guard<std::mutex> lg(m);
        value++;
        if (value == 1)
        {
            cv.notify_all();
        }
    }

    void signal(signal_condition)
    {
        boost::this_thread::at_thread_exit([this]
                                           { this->signal(); });
    }
};

#endif
