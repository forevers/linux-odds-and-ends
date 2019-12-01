// compiler options
// $ g++ -g -O0 -std=c++17 -I../util -pthread -ggdb -lpthread main.cpp ../util/SyncLog.cpp -o promise_future_demo
// $ clang++ -g -O0 -std=c++17 -pthread -lpthread main.cpp ../util/SyncLog.cpp -o promise_future_demo

// simple demo of thread async-future synchronization


#include "SyncLog.h"

#include <functional>
#include <future>
#include <thread>
#include <vector>


enum ProcessingState {
  PROC_STATE_0 = 0,
  PROC_STATE_1 = 1,
  PROC_STATE_2 = 2,
  PROC_STATE_COMPLETED = 3
};


void async_function(int instance_number, std::vector<std::promise<std::pair<int,int>>>& promise_vec)
{
    bool running{true};
    int accum{0};
    int processing_state = int(PROC_STATE_0);


    while (processing_state < PROC_STATE_COMPLETED) {

        accum += 1;

        SyncLog::GetLog()->Log(std::to_string(instance_number) + ": processing_state : " + std::to_string(processing_state));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        promise_vec[processing_state].set_value(std::make_pair(instance_number, accum));
        processing_state += 1;
    }

    promise_vec[processing_state].set_value(std::make_pair(instance_number, PROC_STATE_COMPLETED));

    return;
}


void other_routine()
{
    static int count{0};
    SyncLog::GetLog()->Log("other_routine() call : " + std::to_string(count++));
    std::this_thread::sleep_for(std::chrono::seconds(1));
}


int main() 
{
    {
        std::cout << "FUTURE - PROMISE EXAMPLE" << std::endl;

        // create promise ... can be issued anywhere in a context ... not just return value
        std::vector<std::promise<std::pair<int,int>>> promise_vec;
        std::promise<std::pair<int,int>> promise_state_0;
        std::promise<std::pair<int,int>> promise_state_1;
        std::promise<std::pair<int,int>> promise_state_2;
        std::promise<std::pair<int,int>> promise_state_completed;

        // // futures from promises
        std::future future_state_0 = promise_state_0.get_future();
        std::future future_state_1 = promise_state_1.get_future();
        std::future future_state_2 = promise_state_2.get_future();
        std::future future_state_completed = promise_state_completed.get_future();

        // move ... not copy
        promise_vec.push_back(std::move(promise_state_0));
        promise_vec.push_back(std::move(promise_state_1));
        promise_vec.push_back(std::move(promise_state_2));
        promise_vec.push_back(std::move(promise_state_completed));

        // start async process
        std::thread future_thread(async_function, 0, std::ref(promise_vec));

        // run some other routines ...
        other_routine();

        // blocking get future call
        std::pair<int,int> future_val = future_state_0.get();
        SyncLog::GetLog()->Log(std::to_string(future_val.first) + ": future value : " + std::to_string(future_val.second));
        future_val = future_state_1.get();
        SyncLog::GetLog()->Log(std::to_string(future_val.first) + ": future value : " + std::to_string(future_val.second));
        future_val = future_state_2.get();
        SyncLog::GetLog()->Log(std::to_string(future_val.first) + ": future value : " + std::to_string(future_val.second));
        future_val = future_state_completed.get();
        SyncLog::GetLog()->Log(std::to_string(future_val.first) + ": future value : " + std::to_string(future_val.second));

        if (future_thread.joinable()) future_thread.join();
    }

    return 0;
}
