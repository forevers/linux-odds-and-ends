#pragma once

#include <functional>
#include <string>
#include <sys/resource.h>
#include <thread>

class LinuxThread
{

public:

    // Delete the copy constructor
    LinuxThread(const LinuxThread&) = delete;

    // Delete the Assignment opeartor
    LinuxThread& operator=(const LinuxThread&) = delete;

    // Parameterized Constructor
    LinuxThread(std::function<void()> func, unsigned int affinity = -1);

    // Parameterized Constructor
    LinuxThread(std::function<void(std::string)> func, std::string name, unsigned int affinity = -1);

    // Parameterized Constructor
    LinuxThread(std::function<void(std::string)> func, std::string name, int policy, int priority, unsigned int affinity = -1);
 
    // Move Constructor
    LinuxThread(LinuxThread && obj);
 
    // Move Assignment Operator
    LinuxThread & operator=(LinuxThread && obj);

    // Destructor
    ~LinuxThread() = default;

    void ThreadInfo();

    bool Joinable();

    void Join();

private:

    std::thread thread_;
    std::string name_;
    int affinity_;
};
