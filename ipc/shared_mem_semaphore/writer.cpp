// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -pthread -lrt -o writer writer.cpp
// $ clang++ -g -O0 -std=c++14 -pthread -lrt -o writer writer.cpp


// this app demonstrates a usage of file backed shared memory communication between processes
// one process creates shared mem and semapores for write/read access to shared memory
// an second process blocks for the read semaphore, reads from the file, and signals the write semaphore to release the writer
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
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "shared_mem.h"

using namespace::std;

struct termios tio;
int writer_fd;
/* shared memory resources */
void* shared_mem{nullptr};
SharedData512* shared_data_512{nullptr};
sem_t* sem_write{nullptr};
sem_t* sem_read{nullptr};
size_t byte_size{0};


void ctrl_c_handler(int dummy) {

    // enable buffering
    tcgetattr(STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON;
    tio.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    /* signal reader to terminate */
    shared_data_512->state = STATE_2;
    if (sem_post(sem_read) < 0) {
        cout << "sem_post() failure : " << strerror(errno) << endl;
    }

    munmap(shared_mem, byte_size);
    sem_close(sem_read);
    sem_close(sem_write);
    close(writer_fd);
    unlink(BACKING_FILE);
}


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
    static int file_val{0};
    char data_string[120];
    int sleep_count{0};
    bool running{true};

    /* empty to begin */
    if ( -1 != (writer_fd = shm_open(BACKING_FILE, O_CREAT | O_RDWR, 0666))) {

        byte_size = sizeof(SharedData512);

        /* get a pointer to memory */
        shared_mem = mmap(NULL,     /* kernel chooses the (page-aligned) address at which to create the mapping */
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

                /* write semaphore for writer access */
                sem_write = sem_open(INTER_PROC_SEM_WRITE,  /* semaphore name */
                   O_CREAT,                                 /* create the semaphore */
                   0666,                                    /* access permissions */
                   1);                                      /* initial value */

                if (SEM_FAILED != sem_write) {

                    /* read semaphore for reader access */
                    sem_read = sem_open(INTER_PROC_SEM_READ,  /* semaphore name */
                       O_CREAT,                                 /* create the semaphore */
                       0666,                                    /* access permissions */
                       0);                                      /* initial value */

                    if (SEM_FAILED == sem_read) {
                        munmap(shared_mem, byte_size);
                        sem_close(sem_write);
                        close(writer_fd);
                        cout << "sem_open(INTER_PROC_SEM_READ) failure : " << strerror(errno) << endl;
                        return -1;
                    }
                } else {
                    munmap(shared_mem, byte_size);
                    close(writer_fd);
                    cout << "sem_open(INTER_PROC_SEM_WRITE) failure : " << strerror(errno) << endl;
                    return -1;
                }
            } else {
                munmap(shared_mem, byte_size);
                close(writer_fd);
                cout << "ftruncate() failure : " << strerror(errno) << endl;
                return -1;
            }
        } else {
            close(writer_fd);
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

    signal(SIGINT, ctrl_c_handler);


    while (running && 'q' != (ch = getchar())) {

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


                    /* first entry sem_write is available, after that the read process posts to it when the read is complete */
                    while (-1 == sem_wait(sem_write)) {
                        if (errno == EINTR) {
                            cout << "sem_wait() error: EINTR" << endl;
                            break;
                        }
                    }

                    //* wrelease reader */
                    if (file_val == 10) {
                        shared_data_512->state = STATE_2;
                        running = false;
                    } else {
                        shared_data_512->state = STATE_1;
                    }
                    int num_chars;
                    if (0 <= (num_chars = sprintf(shared_data_512->buffer, "PID: %d, val: %d\n", shared_data_512->pid, file_val))) {

                        cout << "PID: " << shared_data_512->pid << ", state: " << shared_data_512->state << ", val: " << file_val << endl;

                        /* increment the binary semaphore for reader */
                        if (sem_post(sem_read) < 0) {
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

    /* signal reader to terminate */
    shared_data_512->state = STATE_2;
    if (sem_post(sem_read) < 0) {
        cout << "sem_post() failure : " << strerror(errno) << endl;
    }

    munmap(shared_mem, byte_size);
    sem_close(sem_read);
    sem_close(sem_write);
    close(writer_fd);
    unlink(BACKING_FILE);

    cout << "exit" << endl;
}
