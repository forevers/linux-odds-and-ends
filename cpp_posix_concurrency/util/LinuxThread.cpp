// compiler options
// $ g++ -g -O0 -std=c++17 -pthread -ggdb -lpthread -o  ownership_demo main.cpp
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread main.cpp -o ownership_demo

// simple demo of thread ownership transfer

#include "LinuxThread.h"

#include <cstring>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "SyncLog.h"


LinuxThread::LinuxThread(std::function<void()> func, unsigned int affinity) :
    thread_(func)
{
}

LinuxThread::LinuxThread(std::function<void(std::string)> func, std::string name, unsigned int affinity) :
    name_(name),
    affinity_(affinity),
    thread_(func, name)
{
    sched_param scheduler;
    int policy; 

    if (affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(affinity, &cpuset);
        int rc = pthread_setaffinity_np(thread_.native_handle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SyncLog::GetLog()->Log("Error calling pthread_setaffinity_np: " + std::to_string(rc));
        }
    }
}


LinuxThread::LinuxThread(std::function<void(std::string, int, int)> func, std::string name, int policy, int priority, unsigned int affinity) :
    name_(name),
    thread_(func, name, policy, priority)
{
    sched_param scheduler;
    int current_policy; 

    if (affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(affinity, &cpuset);
        int rc = pthread_setaffinity_np(thread_.native_handle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            SyncLog::GetLog()->Log("Error calling pthread_setaffinity_np: " + std::to_string(rc));
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


void LinuxThread::Join()
{
    thread_.join();
}
