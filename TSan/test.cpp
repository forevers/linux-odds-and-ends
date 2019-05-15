// google github
// https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual
// llvm
// https://clang.llvm.org/docs/ThreadSanitizer.html

// clang version modification
// $ sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/lib/llvm-6.0/bin/clang++ 100
// $ sudo update-alternatives --install /usr/bin/clang clang /usr/lib/llvm-6.0/bin/clang 100

// compiler options
// To get a reasonable performance add -O1 or higher.
// Use -g to get file names and line numbers in the warning messages.
// $ g++ -g -O1 -std=c++14  -pthread -fsanitize=thread -fPIE -o test test.cpp
// $ clang++ -g -O1 -std=c++14  -pthread -fsanitize=thread -fPIE -o test test.cpp


#include <iostream>
#include <map>
#include <string>
#include <thread>

using namespace::std;

typedef map<string, string> StringMap;


class ThreadFunctor
{

public:
    void operator()(StringMap& string_map) 
    {
        string_map["functor"] = "functor thread";
    }
};

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
__attribute__((no_sanitize("thread")))
// __attribute__((no_sanitize_thread))
void thread_function(StringMap& string_map)
#endif
#else
void thread_function(StringMap& string_map)
#endif
{
    string_map["function"] = "function thread";
}


int main() 
{
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    cout << "built with ThreadSanitizer" << endl;
#endif
#endif

    StringMap string_map;
    ThreadFunctor thread_functor;

    // thread t(thread_function, map);
    string str{"start"};

    // pass thread string map by reference
    thread thr_function(thread_function, ref(string_map));

    thread thr_functor(thread_functor, ref(string_map));

    // WARNING: ThreadSanitizer: data race (pid=5885)
    string_map["main"] = "main context";

    if (thr_function.joinable()) thr_function.join();
    if (thr_functor.joinable()) thr_functor.join();

    for (auto elem : string_map) {
        cout << elem.second << endl;
    }
}
