// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o writer writer.cpp
// $ clang++ -g -O0 -std=c++14 -o writer writer.cpp


// this app demonstrates a usage of file backed named pipe for fifo communication between processes
// one process creates the named pipe (fifo)
// a second process blocks for the pipe reads


// references
// https://opensource.com/article/19/4/interprocess-communication-linux-channels
// https://linux.die.net/man/3/mkfifo

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "named_pipe.h"

using namespace::std;

struct termios tio;
int writer_fd;


void termination_handler(int dummy) {

    /* close pipe and generated eof for reader */
    close(writer_fd);
    unlink(BACKING_FILE);

    // enable buffering
    tcgetattr(STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON;
    tio.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
}


void print_help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application opens a named pipe fifo for writing to another process" << endl;

    cout << "  <up key> - increment value and write to pipe" << endl;
    cout << "  <up key> - decrement value and write to pipe" << endl;

    cout << "  h - print this menu" << endl;
    cout << "  q - quit" << endl;

    cout << endl;
    cout << "*********************************************************************"  << endl;
}


int main() 
{
    /* create named pipe */
    if (-1 != mkfifo(BACKING_FILE, 0666)) {

        /* create if files does not exist, write only */
        if (-1 != (writer_fd = open(BACKING_FILE, O_CREAT | O_WRONLY))) {

            static int val{0};
            char buffer[512];
            bool running{true};
            pid_t pid;
            int ch;

            print_help();

            // disable buffering
            tcgetattr(STDIN_FILENO, &tio);
            tio.c_lflag &= ~ICANON;
            tio.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &tio);

            signal(SIGINT, termination_handler);

            print_help();

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
                                val++;
                            } else if (66 == key_val) {
                                val--;
                            } else {
                                break;
                            }

                            //* release reader */
                            if (val == 10) running = false;

                            if (running) {

                                int num_chars;
                                if (0 <= (num_chars = sprintf(buffer, "PID: %d, val: %d\n", pid, val))) {

                                    /* pipe write */
                                    write(writer_fd, buffer, strlen(buffer));

                                } else {
                                    cout << "sprintf() failure : " << strerror(errno) << endl;
                                }
                            } else {

                            }
                        }
                        break;

                } /* end switch */
            } /* end while */

            /* release reader and reconfigure terminal */
            termination_handler(0);

        } else {
            cout << "open() failure : " << strerror(errno) << endl;
            return -1;
        }
    } else {
        cout << "mkfifo() failure : " << strerror(errno) << endl;
        return -1;
    }

    cout << "exit writer" << endl;
}
