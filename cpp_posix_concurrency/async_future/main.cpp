// compiler options
// $ g++ -g -O0 -std=c++17 -I../util -pthread -ggdb -lpthread main.cpp ../util/SyncLog.cpp -o async_future_demo
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread main.cpp ../util/SyncLog.cpp -o async_future_demo

// simple demo of thread async-future synchronization


#include "SyncLog.h"

#include <future>
#include <functional>
#include <string>
#include <utility>
#include <vector>

std::pair<int, int>  async_function(int instance_number, int num_stages)
{
    if (0 == num_stages) return std::make_pair(instance_number, 0);

    int run_count{0};
    bool running{true};
    int accum{0};

    while (running) {

        accum += run_count;

        SyncLog::GetLog()->Log(std::to_string(instance_number) + ": async_function pass : " + std::to_string(run_count));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (num_stages == ++run_count) running = false;
    }

    SyncLog::GetLog()->Log("async_function() exiting");

    return std::make_pair(instance_number, accum);
}


void other_routine(int seconds_delay)
{
    static int count{0};
    SyncLog::GetLog()->Log("other_routine() call : " + std::to_string(count++));
    std::this_thread::sleep_for(std::chrono::seconds(seconds_delay));
}


int main() 
{
    {
        std::cout << "FUTURE - ASYNC EXAMPLE" << std::endl;

        // future from async
        std::future<std::pair<int,int>> future_accum = std::async(async_function, 0, 5);

        // run some other routines ...
        other_routine(1);

        // in time blocking get future call
        auto future_val = future_accum.get();
        std::cout << std::to_string(future_val.first) << ": future value : " << std::to_string(future_val.second) << std::endl;
    }

    {
        std::cout << "VECTOR OF FUTURE - ASNYC EXAMPLE" << std::endl;

        std::vector<std::future<std::pair<int,int>>> future_vec;

        // create futures
        for (int i = 0 ; i < 5; i++) {
            // first takes longest
            future_vec.push_back(std::async(async_function, i, 5-i));
        }

        // get futures
        for (auto& future : future_vec) {
            auto future_val = future.get();
            std::cout << std::to_string(future_val.first) << ": future value : " << std::to_string(future_val.second) << std::endl;
        }
    }

    {
        std::cout << "FUTURE - PACKAGED TASK EXAMPLE" << std::endl;

        // lower level sequence using package task

        // async_function ret and params in template
        std::packaged_task<std::pair<int,int>(int,int)> package_task_test(async_function);
        // future from package task
        std::future<std::pair<int,int>> future_accum_packaged = package_task_test.get_future();
        // start packaged task
        package_task_test(0, 5);
        auto future_package_val = future_accum_packaged.get();

        std::cout << future_package_val.first << ": future packaged value : " << future_package_val.second << std::endl;
    }


    return 0;
}
