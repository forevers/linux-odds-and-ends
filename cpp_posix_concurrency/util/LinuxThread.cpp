// compiler options
// $ g++ -g -O0 -std=c++17 -pthread -ggdb -lpthread -o  ownership_demo main.cpp
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread -o main.cpp

// simple demo of thread ownership transfer

#include "LinuxThread.h"

#include <cstring>
#include <errno.h>
// #include <functional>
// #include <iostream>
#include "SyncLog.h"

#include <sys/syscall.h>
#include <unistd.h>


LinuxThread::LinuxThread(std::function<void()> func) :
    thread_(func)
{
}


LinuxThread::LinuxThread(std::function<void(std::string)> func, std::string name) :
    name_(name),
    thread_(func, name)
{
    sched_param scheduler;
    int policy; 

    pthread_getschedparam(thread_.native_handle(), &policy, &scheduler);
    if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

        SyncLog::GetLog()->Log("policy[CFS] : " + std::string(((SCHED_OTHER == policy) ? "SCHED_OTHER" :
            (SCHED_BATCH == policy) ? "SCHED_BATCH" :
             "SCHED_IDLE")));
    }
}


LinuxThread::LinuxThread(std::function<void(std::string)> func, std::string name, int policy, int priority) :
    name_(name),
    thread_(func, name)
{
    sched_param scheduler;
    int current_policy; 

    pthread_getschedparam(thread_.native_handle(), &current_policy, &scheduler);
    if (SCHED_OTHER == policy || SCHED_BATCH == policy || SCHED_IDLE == policy) {

        SyncLog::GetLog()->Log("policy[CFS] : " + std::string(((SCHED_OTHER == policy) ? "SCHED_OTHER" :
            (SCHED_BATCH == policy) ? "SCHED_BATCH" :
             "SCHED_IDLE")));

        pid_t tid;
        tid = syscall(SYS_gettid);
        if (-1 == setpriority(PRIO_PROCESS, tid, priority)) {
            SyncLog::GetLog()->Log("setpriority() failure");
        }

    } else {

        scheduler.sched_priority = priority;
        if (0 == pthread_setschedparam(thread_.native_handle(), policy, &scheduler)) {
            SyncLog::GetLog()->Log("policy[REALTIME] : " + std::string(((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
               (policy == SCHED_RR)    ? "SCHED_RR" :
               "SCHED_OTHER")));
        } else {
            std::cerr << "Failed to set Thread scheduling : " << std::strerror(errno) << std::endl;
        }
    }
}


LinuxThread::LinuxThread(LinuxThread && obj) : 
    thread_(std::move(obj.thread_))
{
    std::cout << "Move Constructor is called" << std::endl;
}


LinuxThread & LinuxThread::operator=(LinuxThread && obj)
{
    std::cout << "Move Assignment is called" << std::endl;
    if (thread_.joinable())
        thread_.join();
    thread_ = std::move(obj.thread_);
    return *this;
}


void LinuxThread::ThreadInfo()
{

}


bool LinuxThread::Joinable()
{
    return thread_.joinable();
}


bool LinuxThread::Join()
{
    thread_.join();
}
