// compiler options
// $ g++ -g -O0 -std=c++17 -pthread -ggdb -lpthread -o scheduler_demo main.cpp
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread main.cpp -o scheduler_demo

// simple demo of CFS and realtime linux threading

// ulimit shows user limits ... for real time priority limits
// $ ulimit -r
// need to run as root for SCHED_FIFO and SCHED_RR scheduling
// $ sudo ./scheduler_demo 
// /etc/security/limits.conf can also be edited to increase a groups realtime priority setting level
// @group - rtprio 65

#include <cstring>
#include <errno.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

using namespace std;


static std::mutex log_mtx_;

void log(std::string msg)
{
    std::lock_guard<std::mutex> lck (log_mtx_);
    std::cout << msg << std::endl;
}


class LinuxThread
{

public:

    // Delete the copy constructor
    LinuxThread(const LinuxThread&) = delete;
 
    // Delete the Assignment opeartor
    LinuxThread& operator=(const LinuxThread&) = delete;
 
    // Parameterized Constructor
    LinuxThread(std::function<void()> func) : thread_(func)
    {}

    // Parameterized Constructor
    LinuxThread(std::function<void(std::string, int, int)> func, std::string name, int policy, int priority) :
        name_(name),
        thread_(func, name, policy, priority)
    {
        sched_param scheduler_params;
        int current_policy; 

        pthread_getschedparam(thread_.native_handle(), &current_policy, &scheduler_params);
        if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

            log("policy[CFS] : " + std::string(((SCHED_OTHER == policy) ? "SCHED_OTHER" :
                (SCHED_BATCH == policy) ? "SCHED_BATCH" :
                 "SCHED_IDLE")));

            pid_t tid;
            tid = syscall(SYS_gettid);
            if (-1 == setpriority(PRIO_PROCESS, tid, priority)) {
                log("setpriority() failure");
            }

        } else {

            scheduler_params.sched_priority = priority;
            if (0 == pthread_setschedparam(thread_.native_handle(), policy, &scheduler_params)) {
                log("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                   (policy == SCHED_RR)    ? "SCHED_RR" :
                   "SCHED_OTHER")));
            } else {
                std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
            }
        }
    }
 
    // Move Constructor
    LinuxThread(LinuxThread && obj) : thread_(std::move(obj.thread_))
    {
        std::cout << "Move Constructor is called" << std::endl;
    }
 
    // Move Assignment Operator
    LinuxThread & operator=(LinuxThread && obj)
    {
        std::cout << "Move Assignment is called" << std::endl;
        if (thread_.joinable())
            thread_.join();
        thread_ = std::move(obj.thread_);
        return *this;
    }

    // Destructor
    ~LinuxThread() = default;

    bool joinable()
    {
        return thread_.joinable();
    }

    void join()
    {
        thread_.join();
    }

private:

    std::thread thread_;
    std::string name_;
};


void thread_1_handler(std::string name, int policy, int priority)
{
    sched_param scheduler_params;
    int current_policy; 

    pthread_getschedparam(pthread_self(), &current_policy, &scheduler_params);

    // configure priority/niceness
    if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

        log("policy[CFS] : " + std::string(((SCHED_OTHER == policy) ? "SCHED_OTHER" :
            (SCHED_BATCH == policy) ? "SCHED_BATCH" :
                "SCHED_IDLE")));

        /* set policy */
        if (0 == pthread_setschedparam(pthread_self(), policy, &scheduler_params)) {
            log("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                (policy == SCHED_RR)    ? "SCHED_RR" :
                "SCHED_OTHER")));
        } else {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }

        pid_t tid;
        tid = syscall(SYS_gettid);
        /* set niceness */
        if (-1 == setpriority(PRIO_PROCESS, tid, priority)) {
            log("setpriority() failure");
        }

    } else {

        /* set priority */
        scheduler_params.sched_priority = priority;
        if (0 == pthread_setschedparam(pthread_self(), policy, &scheduler_params)) {
            log("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                (policy == SCHED_RR)    ? "SCHED_RR" :
                "SCHED_OTHER")));
        } else {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }
    }

    int run_count{0};
    bool running{true};

    while (running) {

        pthread_getschedparam(pthread_self(), &current_policy, &scheduler_params);

        if (SCHED_OTHER == current_policy || SCHED_BATCH == current_policy || SCHED_IDLE == current_policy) {

            pid_t tid;
            tid = syscall(SYS_gettid);
            int niceness = getpriority(PRIO_PROCESS, tid);

            log("thread " + name + ", id = " + std::to_string(tid) + ", niceness = " + std::to_string(niceness)
                + ", policy = " + std::string(((current_policy == SCHED_OTHER)  ? "SCHED_OTHER" :
                    (current_policy == SCHED_BATCH)    ? "SCHED_BATCH" :
                    (current_policy == SCHED_IDLE) ? "SCHED_IDLE" :
                    "???")));
        } else {

            std::stringstream ss;
            ss << std::this_thread::get_id();
            log("thread " + name + ", id = " + ss.str() + ", priority = " + std::to_string(scheduler_params.sched_priority) 
                + ", policy = " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                    (policy == SCHED_RR)    ? "SCHED_RR" :
                    (policy == SCHED_OTHER) ? "SCHED_OTHER" :
                    "???")));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (2 == run_count++) running = false;
    }
}


int main() 
{
    cout<<"SCHED_OTHER max priority : "<<sched_get_priority_max(SCHED_OTHER)<<endl;
    cout<<"SCHED_BATCH max priority : "<<sched_get_priority_max(SCHED_BATCH)<<endl;
    cout<<"SCHED_IDLE max priority : "<<sched_get_priority_max(SCHED_IDLE)<<endl;
    cout<<"SCHED_IDLE max priority : "<<sched_get_priority_max(SCHED_FIFO)<<endl;
    cout<<"SCHED_RR max priority : "<<sched_get_priority_max(SCHED_RR)<<endl;

    thread thread_1(thread_1_handler, "thread_1", 0, 0);

    /* For processes scheduled under one of the normal scheduling policies (SCHED_OTHER, SCHED_IDLE, SCHED_BATCH),
       sched_priority is not used in scheduling decisions (it must be specified as 0). Instead use setpriority()
       to configure niceness level.
    */

    // SCHED_OTHER - the standard round-robin time-sharing policy;
    LinuxThread thread_linux_other = LinuxThread(thread_1_handler, "thread_linux_other", SCHED_OTHER, 3);
    // SCHED_BATCH - for "batch" style execution of processes
    LinuxThread thread_linux_batch = LinuxThread(thread_1_handler, "thread_linux_batch", SCHED_BATCH, 4);
    // SCHED_IDLE - for running very low priority background jobs.
    LinuxThread thread_linux_idle = LinuxThread(thread_1_handler, "thread_linux_idle", SCHED_IDLE, 5);

    /* The following "real-time" policies are also supported, for special time-critical applications that need precise 
       control over the way in which runnable processes are selected for execution
    */

    // SCHED_FIFO - a first-in, first-out policy
    LinuxThread thread_linux_fifo = LinuxThread(thread_1_handler, "thread_linux_fifo", SCHED_FIFO, 1);
    // SCHED_RR - a round-robin policy
    LinuxThread thread_linux_rr = LinuxThread(thread_1_handler, "thread_linux_rr", SCHED_RR, 2);

    if (thread_1.joinable()) thread_1.join();
    if (thread_linux_other.joinable()) thread_linux_other.join();
    if (thread_linux_batch.joinable()) thread_linux_batch.join();
    if (thread_linux_idle.joinable()) thread_linux_idle.join();
    if (thread_linux_fifo.joinable()) thread_linux_fifo.join();
    if (thread_linux_rr.joinable()) thread_linux_rr.join();

    return 0;
}
