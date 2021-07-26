/*
    native compiler options
        g++ -g -O0 -std=c++17 -ggdb -o memory_test main.cpp

    references :
        /proc/zoneinfo
*/


#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>
#include "../utils/exec-shell.h"

#include "memory.h"

using namespace::std;

static int mmap_buffer_size_{-1};
char* kernel_buff_{nullptr};
static uint8_t fill_val_{0};
static size_t fill_offset_{0};

void print_key_processing()
{
    cout<<endl<<"****************************** Main Menu ******************************"<<endl;
    cout<<endl;
    cout<<" This application tests read, write, and mmap access to kernel buffer"<<endl;
    cout<<endl;
    cout<<"  q - quit"<<endl;
    cout<<"  f - zero fill buffer"<<endl;
    cout<<"  d - dump buffer"<<endl;
    cout<<"  r - report buffer info"<<endl;
    cout<<"  <up key> - ..."<<endl;
    cout<<"  <down key> - ..."<<endl;
    cout<<"  <left key> - ..."<<endl;
    cout<<"  <right key> - ..."<<endl;
    cout<<"  h - Print this menu"<<endl;
    cout<<endl;
    cout<<"*********************************************************************" <<endl;
}


static void process_key_entry(int fd)
{
    int ch;
    struct termios t;

    print_key_processing();

    /* disable buffering */
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    while ('q' != (ch = getchar())) {
        switch (ch) {

            case 'h': {
                print_key_processing();
                break;
            }

            /* dump buffer */
            case 'd': {
                for (int i = 0; i < 16; i++) {
                    cout<<hex<<unsigned(kernel_buff_[i]);
                }
                cout<<endl;
                break;
            }

            case 'f': {
                /* fill the buffer */
                break;
            }

            case 27: {

                if (91 == getchar()) {
                    int key_val = getchar();
                    if (66 == key_val) {
                        /* down arrow */
                        if (fill_offset_) {
                            kernel_buff_[fill_offset_--] = fill_val_;
                            cout<<std::hex<<fill_offset_<<":"<<hex<<unsigned(fill_val_)<<endl;
                        }
                    } else if (65 == key_val) {
                        /* up arrow */
                        if (fill_offset_ < mmap_buffer_size_-1) {
                            kernel_buff_[fill_offset_++] = fill_val_;
                            cout<<std::hex<<fill_offset_<<":"<<unsigned(fill_val_)<<endl;
                        }
                    } else if (67 == key_val) {
                        /* right arrow */
                        fill_val_++;
                        cout<<std::hex<<unsigned(fill_val_)<<endl;
                    } else if (68 == key_val) {
                        /* left arrow */
                        fill_val_--;
                        cout<<std::hex<<unsigned(fill_val_)<<endl;
                    } else {
                        break;
                    }
                }
                break;
            }

            default:
                break;

        } /* end switch ch */
    } /* end while */

    /* enable buffering */
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON;
    t.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}


int main() 
{
    int fd;

    /* open module handle*/
    if (-1 != (fd = open("/dev/ess-device-name", O_RDWR))) {

        cout<<"open() success"<<endl;

        string proc_line;
        ifstream proc_file("/proc/ess-proc-name");
        if (proc_file.is_open()) {
            while (getline(proc_file, proc_line)) {
                size_t proc_entry = proc_line.find("size = ");
                if (0 == proc_entry) {
                    mmap_buffer_size_ = stoi(proc_line.substr(strlen("size = ")));
                    cout<<"mmap buffer size: "<<mmap_buffer_size_<<endl;
                }
            }
            proc_file.close();
        }

        if (mmap_buffer_size_ > 0) {

            cout<<"mmap the buffer"<<endl;
            kernel_buff_ = (char*)mmap(0,
                    mmap_buffer_size_,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,  
                    fd,
                    0);

            process_key_entry(fd);

            /* unmap */
            munmap(kernel_buff_, mmap_buffer_size_);
        }

        /* close module handle */
        close(fd);

    } else {
        cout<<"open() failure : "<<strerror(errno)<<endl;
        return -1;
    }

    cout<<"exit"<<endl;

    return 0;
}
