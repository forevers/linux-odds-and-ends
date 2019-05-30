// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o reader reader.cpp
// $ clang++ -g -O0 -std=c++14 -o reader reader.cpp

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

using namespace::std;

void print_help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application opens, locks, and reads a local file" << endl;

    cout << "  <up key> - open file, lock file, read value, unlock file" << endl;
    cout << "  <down key> - open file, lock file, read value, unlock file" << endl << endl;

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
    char ipc_filename[] = "ipc_file.lock";
    static int file_val{0};
    char data_string[120];

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
                    if (65 != key_val && 66 != key_val) break;

                    bool increment_file;
                    struct flock lock;

                    if (-1 != (reader_fd = open(ipc_filename, O_RDONLY))) {

                        // configure the lock
                        lock.l_type = F_WRLCK;    /* read/write (exclusive) lock */
                        lock.l_whence = SEEK_SET; /* base for seek offsets */
                        lock.l_start = 0;         /* 1st byte in file */
                        lock.l_len = 0;           /* 0 here means 'until EOF' */
                        lock.l_pid = getpid();    /* process id */

                        // lock.l_type = F_WRLCK;
                        lock.l_type = F_RDLCK;
                        int ret;
                        if (-1 != (ret = fcntl(reader_fd, F_SETLK, &lock))) {

                            /* read chars from file */
                            char c; /* buffer for read bytes */
                            char c_buff[10];
                            ssize_t num_read{0};
                            ssize_t num_read_total{0};
                            /* num_read = 0 is eof */
                            int string_idx{0};
                            while (1 == (num_read = read(reader_fd, &c, 1))) {

                                num_read_total += num_read;
                                cout << c;
                            }

                            cout << "num_read_total :" << num_read_total << ", num_read : " << num_read << endl;

                        } else {
                            cout << "fcntl(F_SETLK) failure : " << strerror(errno) << endl;
                        }

                        // close
                        if (-1 == close(reader_fd)) cout << "writer_fd() failure" << endl;

                    } else {
                        cout << "open() failure" << endl;
                        cout << "err : " << strerror(errno) << endl;
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
