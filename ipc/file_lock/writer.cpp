// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o writer writer.cpp
// $ clang++ -g -O0 -std=c++14 -o writer writer.cpp


// this app demonstrates an Advisory Locking impl between to processes.
// One process writes to a file, and another process reads from the file
// in an unsynced mannor.
// https://www.thegeekstuff.com/2012/04/linux-file-locking-types/

//fopen()/flock()
// http://man7.org/linux/man-pages/man3/flockfile.3.html
// https://gavv.github.io/articles/file-locks/#posix-record-locks-fcntl


#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

using namespace::std;

void print_help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application opens, locks, and modifies a local file" << endl;

    cout << "  <up key> - open file, lock file, increment value, unlock file" << endl;
    cout << "  <down key> - open file, lock file, decrement value, unlock file" << endl << endl;

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
    char ipc_filename[] = "ipc_file.lock";
    static int file_val{0};
    char data_string[120];
    int sleep_count{0};

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

                    bool increment_file;
                    struct flock lock;

                    if (0 != (writer_fd = open(ipc_filename, O_WRONLY | O_CREAT, 0666))) {

                        // configure the lock
                        lock.l_type = F_WRLCK;      /* read/write (exclusive) lock */
                        lock.l_start = 0;           /* 1st byte in file */
                        lock.l_whence = SEEK_SET;   /* base for seek offsets */
                        lock.l_len = 0;             /* 0 here means 'until EOF' */
                        lock.l_pid = getpid();      /* process id */

                        /** F_SETLKW blocking 
                            F_SETLK non blocking 
                         **/

                        /** TODO
                            test file range locking
                            https://gavv.github.io/articles/file-locks/#posix-record-locks-fcntl
                        */
                        if (-1 != fcntl(writer_fd, F_SETLKW, &lock)) {

                            cout << "lock hold" << endl;

                            int num_chars;
                            if (0 <= (num_chars = sprintf(data_string, "PID: %d, val: %d\n", lock.l_pid, file_val))) {

                                write(writer_fd, data_string, num_chars+1);
                                cout << "PID: " << lock.l_pid << " val: " << file_val << endl;

                                if ( ++sleep_count % 2) {
                                    cout << "5 second lock hold" << endl;
                                    sleep(5);
                                }

                                // unlock
                                lock.l_type = F_UNLCK;
                                if (fcntl(writer_fd, F_SETLK, &lock) < 0) cout << "fcntl() failure" << endl;

                                cout << "lock released" << endl;

                                // close
                                if (-1 == close(writer_fd)) cout << "writer_fd() failure" << endl;

                            } else {
                                cout << "data_string() failure" << endl;
                            } 
                        } else {
                            cout << " fcntl(F_SETLKW) failure" << endl;
                        }
                    } else {
                        cout << "open() failure" << endl;
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
