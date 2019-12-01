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
    LinuxThread(std::function<void()> func);

    // Parameterized Constructor
    LinuxThread(std::function<void(std::string)> func, std::string name);

    // Parameterized Constructor
    LinuxThread(std::function<void(std::string)> func, std::string name, int policy, int priority);
 
    // Move Constructor
    LinuxThread(LinuxThread && obj);
 
    // Move Assignment Operator
    LinuxThread & operator=(LinuxThread && obj);

    // Destructor
    ~LinuxThread() = default;

    void ThreadInfo();

    bool Joinable();

    bool Join();

private:

    std::thread thread_;
    std::string name_;
};
