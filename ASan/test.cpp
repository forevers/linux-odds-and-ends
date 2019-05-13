// google github
// https://github.com/google/sanitizers/wiki/AddressSanitizer

// compiler options
// $ g++ -g -O0-std=c++14 -fsanitize=address  -ggdb -o test test.cpp
// $ g++ -g -O0-std=c++14 -fsanitize-address-use-after-scope -ggdb -o test test.cpp
//
// $ clang++-3.5 -g -O0 -std=c++14 -fsanitize=address -o test test.cpp
// $ clang++-3.5 -g -O0 -std=c++14 -fsanitize-address-use-after-scope -o test test.cpp

// We can force ASan to crash software when an error happens with the environment variable ASAN_OPTIONS like this
// $ export ASAN_OPTIONS='abort_on_error=1'

#include <iostream>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

using namespace::std;

#define ARRAY_SIZE 100 
int array_[ARRAY_SIZE];

int *ptr_global_;

enum class TestCase {
    ARRAY_ACCESS_STACK,
    ARRAY_ACCESS_GLOBAL,
};

void escape_local_object() {
    int local[100];
    ptr_global_ = &local[0];
}


void help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application demonstrates a few ASan test cases" << endl;
    cout << " Select one of the following test cases using the keypad" << endl;
    cout << endl;

    cout << "  q - quit" << endl;
    cout << "  a - stack array access out of range" << endl;
    cout << "  b - global array access out of range" << endl;
    cout << "  c - access already deleted array" << endl;
    cout << "  d - access out of range access of new'ed object" << endl;
    cout << "  e - aaccess out of scope return pointer" << endl;
    cout << "  f - access after scope" << endl;
    cout << "  g - leek detection" << endl;

    cout << "  h - print this menu" << endl;
    cout << endl;
    cout << "*********************************************************************"  << endl;
}


static void process_key_entry()
{
    int ch;
    struct termios t;

    help();

    // disable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    while ('q' != (ch = getchar())) {

        switch (ch) {
            case 'h':
                help();
                break;
            case 'a':
                cout << "stack array access out of range" << endl;
                {
                    // AddressSanitizer: stack-buffer-overflow
                    int a[2] = {1, 0};
                    // force range error
                    int val = a[2];
                    cout << val << endl;
                }
                break;
            case 'b':
                cout << "global array access out of range" << endl;
                {
                    // AddressSanitizer: global-buffer-overflow 
                    int val = array_[ARRAY_SIZE];
                    cout << val << endl;
                }
                break;
            case 'c':
                cout << "access after new/delete sequence" << endl;
                {
                    // ERROR: AddressSanitizer: heap-use-after-free
                    int* a = new int[10];
                    delete [] a;
                    // force range error
                    int val = a[2];
                    cout << val << endl;
                }
            case 'd':
                cout << "access out of range access of new'ed object" << endl;
                {
                    // AddressSanitizer: heap-buffer-overflow
                    int* a = new int[10];
                    // force range error
                    int val = a[12];
                    cout << val << endl;
                    delete [] a;
                }
            case 'e':
                cout << "access out of scope global pointer" << endl;
                {
                    // RUN: clang -O -g -fsanitize=address %t && ./a.out
                    // By default, AddressSanitizer does not try to detect
                    // stack-use-after-return bugs.
                    // It may still find such bugs occasionally
                    // and report them as a hard-to-explain stack-buffer-overflow.
                    // You need to run the test with ASAN_OPTIONS=detect_stack_use_after_return=1
                    escape_local_object();


                    int val = ptr_global_[1];
                    cout << "could not get this error to trigger asan " << val << endl;
                }
                break;
            case 'f':
                cout << "access after scope" << endl;
                {
                    // -fsanitize-address-use-after-scope
                    // NOTE could not compile with above option
                    //    g++ (Ubuntu 5.4.0-6ubuntu1~16.04.11) 5.4.0 20160609
                    //    Ubuntu clang version 3.5.2-3ubuntu1 (tags/RELEASE_352/final) (based on LLVM 3.5.2
                    {
                        int x = 0;
                        ptr_global_ = &x;
                    }
                    *ptr_global_ = 5;
                    cout << "could not get this error to trigger asan " << *ptr_global_ << endl;
                }
                break;
            case 'g':
                cout << "leek detection" << endl;
                {
                    // enabled by default .... to disable
                        // ASAN_OPTIONS=detect_leaks=1
                    // ERROR: LeakSanitizer: detected memory leaks
                    ptr_global_ = new int[7];
                    ptr_global_ = 0;
                }
                break;
            default:
                break;
        }

        if (ch == 'g') break;
    }

    // enable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON;
    t.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}



int main()
{
    process_key_entry();

    return 0;
}

