// compiler options
// $ g++ -g -O0 -std=c++17 -I../util -pthread -ggdb -lpthread main.cpp ../util/LinuxThread.cpp ../util/SyncLog.cpp -o allocation__affinity_demo
// $ clang++ -g -O0 -std=c++17 -I../util -pthread -lpthread main.cpp ../util/LinuxThread.cpp ../util/SyncLog.cpp -o allocation_affinity_demo

// simple demo of affinity assigned thread collection construct destruct

#include "LinuxThread.h"

#include "SyncLog.h"

#include <cstring>
#include <sched.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>


void thread_handler(std::string name)
{
    int run_count{0};
    bool running{true};

    sched_param scheduler_params;
    int current_policy; 
    int current_priority; 

    // configure priority/niceness
    pthread_getschedparam(pthread_self(), &current_policy, &scheduler_params);

    while (running) {

        if (SCHED_OTHER == current_priority || SCHED_BATCH == current_priority || SCHED_IDLE == current_priority) {

            pid_t tid;
            tid = syscall(SYS_gettid);
            int niceness = getpriority(PRIO_PROCESS, tid);

            SyncLog::GetLog()->Log("thread " + name + ", id = " + std::to_string(tid) 
                + ", cores id = " + std::to_string(sched_getcpu()) 
                + ", priority = " + std::to_string(current_priority) 
                + ", niceness = " + std::to_string(niceness)
                + ", policy = " + std::string(((current_policy == SCHED_OTHER)  ? "SCHED_OTHER" :
                    (current_policy == SCHED_BATCH)    ? "SCHED_BATCH" :
                    (current_policy == SCHED_IDLE) ? "SCHED_IDLE" :
                    "???")));
        } else {

            std::stringstream ss;
            ss << std::this_thread::get_id();
            SyncLog::GetLog()->Log("thread " + name + ", id = " + ss.str() 
                + ", cores id = " + std::to_string(sched_getcpu()) 
                + ", priority = " + std::to_string(current_priority) 
                + ", policy = " + std::string(((current_priority == SCHED_FIFO)  ? "SCHED_FIFO" :
                    (current_policy == SCHED_RR)    ? "SCHED_RR" :
                    (current_policy == SCHED_OTHER) ? "SCHED_OTHER" :
                    "???")));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (5 == run_count++) running = false;
    }
}

void thread_handler_priority(std::string name, int policy, int priority)
{
    sched_param scheduler_params;
    int current_policy; 

    // configure priority/niceness
    pthread_getschedparam(pthread_self(), &current_policy, &scheduler_params);

    if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

        SyncLog::GetLog()->Log("current policy[CFS] : " + std::string(((SCHED_OTHER == current_policy) ? "SCHED_OTHER" :
                (SCHED_BATCH == policy) ? "SCHED_BATCH" :
                    "SCHED_IDLE")));

        /* set policy */
        if (0 == pthread_setschedparam(pthread_self(), policy, &scheduler_params)) {
            SyncLog::GetLog()->Log("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                (policy == SCHED_RR)    ? "SCHED_RR" :
                "SCHED_OTHER")));
        } else {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }

        pid_t tid;
        tid = syscall(SYS_gettid);
        /* set niceness */
        if (-1 == setpriority(PRIO_PROCESS, tid, priority)) {
            SyncLog::GetLog()->Log(("setpriority() failure"));
        }
    
    } else {

        /* set priority */
        scheduler_params.sched_priority = priority;
        if (0 == pthread_setschedparam(pthread_self(), policy, &scheduler_params)) {
             SyncLog::GetLog()->Log(("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                (policy == SCHED_RR)    ? "SCHED_RR" :
                "SCHED_OTHER"))));
        } else {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }
    }

    int run_count{0};
    bool running{true};

    while (running) {

        pthread_getschedparam(pthread_self(), &policy, &scheduler_params);

        if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

            pid_t tid;
            tid = syscall(SYS_gettid);
            int niceness = getpriority(PRIO_PROCESS, tid);

            SyncLog::GetLog()->Log("thread " + name + ", id = " + std::to_string(tid) + ", niceness = " + std::to_string(niceness)
                + ", policy = " + std::string(((current_policy == SCHED_OTHER)  ? "SCHED_OTHER" :
                    (current_policy == SCHED_BATCH)    ? "SCHED_BATCH" :
                    (current_policy == SCHED_IDLE) ? "SCHED_IDLE" :
                    "???")));
        } else {

            std::stringstream ss;
            ss << std::this_thread::get_id();
            SyncLog::GetLog()->Log("thread " + name + ", id = " + ss.str() + ", cores id = " + std::to_string(sched_getcpu()) +  ", priority = " + std::to_string(scheduler_params.sched_priority) 
                + ", policy = " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                    (policy == SCHED_RR)    ? "SCHED_RR" :
                    (policy == SCHED_OTHER) ? "SCHED_OTHER" :
                    "???")));
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (5 == run_count++) running = false;
    }
}

int main() 
{
    // number of concurrenty threads platform supports ( $ cat /proc/cpuinfo )
    auto num_cores = std::thread::hardware_concurrency();

    std::cout << "hardware_concurrency() : " << num_cores << std::endl;

    std::vector<LinuxThread> linux_threads;
    for (int idx = 0; idx < num_cores; idx++) {
        // uses move constructor
        linux_threads.push_back(LinuxThread(thread_handler_priority, "thread_" + std::to_string(idx), SCHED_OTHER, idx, idx));
    }

    for (auto& thread_elem : linux_threads) thread_elem.Join();

    return 0;
}
