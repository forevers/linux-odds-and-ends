// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -pthread -lrt -o writer writer.cpp
// $ clang++ -g -O0 -std=c++14 -pthread -lrt -o writer writer.cpp


// this app demonstrates a usage of file backed shared memory communication between processes
// one process creates shared mem and semapore for write access to shared memory
// an second process blocks for the semaphore and reads from the file
// an os managed semaphore may be used as a process access lock or a mutex inside the shared memory space may also be used
//   ...mutex init/access sequence important
//   ...com could also occur through a UNIX socket to statefully configure the shared memory
//   ...possibly passing the shared memory region file descriptor through the socket with a SCM_RIGHTS message

// references
// https://opensource.com/article/19/4/interprocess-communication-linux-storage
// https://stackoverflow.com/questions/42628949/using-pthread-mutex-shared-between-processes-correctly


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
    cout << " This application opens shared memory, modifies it and signals another process" << endl;

    cout << "  <up key> - lock binary semaphore, open memory, increment value, release semaphore" << endl;
    cout << "  <up key> - lock binary semaphore, open memory, decrement value, release semaphore" << endl;

    cout << "  h - print this menu" << endl;
    cout << "  q - quit" << endl;

    cout << endl;
    cout << "*********************************************************************"  << endl;
}



int main() 
{
    int ch;
    int writer_fd;
    struct termios tio;
    static int file_val{0};
    char data_string[120];
    int sleep_count{0};
    sem_t* semaphore{nullptr};

    /* create shared memory resources */
    SharedData512* shared_data_512{nullptr};

    /* empty to begin */
    if ( -1 != (writer_fd = shm_open(BACKING_FILE, O_CREAT | O_RDWR, 0666))) {

        size_t byte_size = sizeof(SharedData512);

        /* get a pointer to memory */
        void* shared_mem = mmap(NULL,     /* kernel chooses the (page-aligned) address at which to create the mapping */
            byte_size,
            PROT_READ | PROT_WRITE,   /* read and write access */
            MAP_SHARED,               /* updates visible to other processes */
            writer_fd,
            0);                       /* start at beginning of file*/

        if (MAP_FAILED != shared_mem) {

            cout << "shared mem address: " << byte_size << "[0.." << (byte_size - 1) << "]" << endl;

            if (-1 != ftruncate(writer_fd, byte_size)) {

                shared_data_512 = static_cast<SharedData512*>(shared_mem);
                shared_data_512->pid = getpid();
                shared_data_512->state = STATE_0;

                /* create a binary semaphore for mutual exclusion with other process*/
                semaphore = sem_open(INTER_PROC_SEM, /* semaphore name */
                   O_CREAT,                          /* create the semaphore */
                   0666,                             /* access permissions */
                   0);                               /* initial value */

                if (SEM_FAILED == semaphore) {
                    cout << "sem_open() failure : " << strerror(errno) << endl;
                    return -1;
                }
            } else {
                cout << "ftruncate() failure : " << strerror(errno) << endl;
                return -1;
            }
        } else {
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

    while ('q' != (ch = getchar())) {

        switch (ch) {

            case 'h':
                print_help();
                break;

            case 27:
                // up/down arrow
                if (91 == getchar()) {

                    // if up or down key, modify val and write to lock file
                    int key_val = getchar();
                    if (65 == key_val) {
                        file_val++;
                    } else if (66 == key_val) {
                        file_val--;
                    } else {
                        break;
                    }

                    shared_data_512->state = STATE_1;
                    int num_chars;
                    if (0 <= (num_chars = sprintf(shared_data_512->buffer, "PID: %d, val: %d\n", shared_data_512->pid, file_val))) {

                        cout << "PID: " << shared_data_512->pid << ", state: " << shared_data_512->state << ", val: " << file_val << endl;

                        /* down arrow stalls the post signalling to reader */
                        if (66 == key_val) sleep(10);

                        /* increment the binary semaphore for reader */
                        if (sem_post(semaphore) < 0) {
                            cout << "sem_post() failure : " << strerror(errno) << endl;
                        }

                    } else {
                        cout << "sprintf() failure : " << strerror(errno) << endl;
                    }
                }
                break;

        } // end switch
    }

    // enable buffering
    tcgetattr(STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON;
    tio.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    cout << "exit" << endl;
}
