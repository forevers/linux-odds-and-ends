// compiler options
// $ g++ -g -O0 -std=c++17 -I../util -pthread -ggdb -lpthread main.cpp ../util/LinuxThread.cpp ../util/SyncLog.cpp -o ownership_demo
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread main.cpp ../util/LinuxThread.cpp ../util/SyncLog.cpp -o ownership_demo

// simple demo of thread ownership transfer

#include "LinuxThread.h"

#include "SyncLog.h"

#include <sstream>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>


void thread_handler(std::string name)
{
    int run_count{0};
    bool running{true};

    sched_param sch;
    int policy; 

    while (running) {

        pthread_getschedparam(pthread_self(), &policy, &sch);

        if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

            pid_t tid;
            tid = syscall(SYS_gettid);
            int priority = getpriority(PRIO_PROCESS, tid);

            SyncLog::GetLog()->Log("thread " + name + ", id = " + std::to_string(tid) + ", priority = " + std::to_string(priority)
                + ", policy = " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                    (policy == SCHED_RR)    ? "SCHED_RR" :
                    (policy == SCHED_OTHER) ? "SCHED_OTHER" :
                    "???")));
        } else {

            std::stringstream ss;
            ss << std::this_thread::get_id();
            SyncLog::GetLog()->Log("thread " + name + ", id = " + ss.str() + ", priority = " + std::to_string(sch.sched_priority) 
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
    std::cout << "hardware concurrency : " << std::thread::hardware_concurrency() << std::endl;

    LinuxThread thread_1(thread_handler, "thread_1");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    LinuxThread thread_2 = std::move(thread_1);

    if (thread_1.Joinable()) thread_1.Join();
    if (thread_2.Joinable()) thread_2.Join();

    return 0;
}
