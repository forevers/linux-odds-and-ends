// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -pthread -lrt -o reader reader.cpp
// $ clang++ -g -O0 -std=c++14 -pthread -lrt -o reader reader.cpp

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#include "shared_mem.h"

using namespace::std;

void print_help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application opens shared memory, access semaphore lock, reads memory and releases semaphore" << endl;

    cout << "  <up key> - lock binary semaphore, open memory, read value, release semaphore" << endl;
    cout << "  <up key> - lock binary semaphore, open memory, read value, release semaphore" << endl;

    cout << "  h - print this menu" << endl;
    cout << "  q - quit" << endl;

    cout << endl;
    cout << "*********************************************************************"  << endl;
}



int main() 
{
    int ch;
    int reader_fd;
    struct termios tio;
    static int file_val{0};
    char data_string[120];
    sem_t* sem_write{nullptr};
    sem_t* sem_read{nullptr};
    void* shared_mem{nullptr};
    int byte_size{0};
    bool running{true};
    /* create shared memory resources */
    SharedData512* shared_data_512{nullptr};

    /* open shared memory */
    if ( -1 != (reader_fd = shm_open(BACKING_FILE, O_RDWR, 0666))) {

        byte_size = sizeof(SharedData512);

        /* get a pointer to memory */
        shared_mem = mmap(NULL,     /* kernel chooses the (page-aligned) address at which to create the mapping */
            byte_size,
            PROT_READ | PROT_WRITE,   /* read and write access */
            MAP_SHARED,               /* updates visible to other processes */
            reader_fd,
            0);                       /* start at beginning of file*/

        if (MAP_FAILED != shared_mem) {

            cout << "shared mem address: " << byte_size << "[0.." << (byte_size - 1) << "]" << endl;

            shared_data_512 = static_cast<SharedData512*>(shared_mem);

            /* create a binary semaphore for mutual exclusion with other process*/
            sem_read = sem_open(INTER_PROC_SEM_READ,    /* semaphore name */
               O_CREAT,                                 /* create the semaphore */
               0666,                                    /* access permissions */
               0);                                      /* initial value */

            if (SEM_FAILED != sem_read) {

                 /* read semaphore for reader access */
                sem_write = sem_open(INTER_PROC_SEM_WRITE,  /* semaphore name */
                   O_CREAT,                                 /* create the semaphore */
                   0666,                                    /* access permissions */
                   0);                                      /* initial value */

                if (SEM_FAILED == sem_read) {
                    sem_close(sem_read);
                    munmap(shared_mem, byte_size);
                    close(reader_fd);
                    cout << "sem_open(INTER_PROC_SEM_WRITE) failure : " << strerror(errno) << endl;
                    return -1;
                }
            } else {
                munmap(shared_mem, byte_size);
                close(reader_fd);
                cout << "sem_open(INTER_PROC_SEM_READ) failure : " << strerror(errno) << endl;
                return -1;
            }
        } else {
            close(reader_fd);
            cout << "mmap() failure : " << strerror(errno) << endl;
            return -1;
        }
    } else {
        cout << "shm_open() failure : " << strerror(errno) << endl;
        return -1;
    }

    print_help();

    // disable buffering
    tcgetattr(STDIN_FILENO, &tio);
    tio.c_lflag &= ~ICANON;
    tio.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    // todo termination string for process termintation
    while (running) {

        /* use semaphore as a mutex (lock) by waiting for writer to increment it */
        while (-1 == sem_wait(sem_read)) {
            if (errno == EINTR) {
                cout << "sem_wait() error: EINTR" << endl;
                break;
            }
        }

        cout << "PID: " << shared_data_512->pid << ", state: " << shared_data_512->state << ", val: " << file_val << endl;
        cout << "buffer: " << shared_data_512->buffer << endl;

        /* treat state 2 as termination state */
        if (shared_data_512->state == STATE_2) {
            running = false;
        }
        /* increment the binary semaphore for writer access */
        if (sem_post(sem_write) < 0) {
            cout << "sem_post() failure : " << strerror(errno) << endl;
        }
    }

    /* release resources */
    sem_close(sem_read);
    sem_close(sem_write);
    munmap(shared_mem, byte_size);
    close(reader_fd);
    unlink(BACKING_FILE);

    // enable buffering
    tcgetattr(STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON;
    tio.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    cout << "exit" << endl;
}
