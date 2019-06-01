// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o pipe_unnamed pipe_unnamed.cpp
// $ clang++ -g -O0 -std=c++14 -o pipe_unnamed pipe_unnamed.cpp

// this app demonstrates a usage of unnamed pipe com between a parent and child process

// references
// https://www.tldp.org/LDP/lpg/node11.html
// https://opensource.com/article/19/4/interprocess-communication-linux-channels
// https://linux.die.net/man/2/pipe
// https://linux.die.net/man/2/_exit
// http://man7.org/linux/man-pages/man2/fork.2.html
// http://man7.org/linux/man-pages/man2/wait.2.html

#include <iostream>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace::std;

enum PipePort {
    READ_PORT,
    WRITE_PORT
};

struct termios tio;
bool parent_running{true};
int pipe_fd[2];
pid_t pid;


void ctrl_c_handler(int dummy) {

    close(pipe_fd[WRITE_PORT]);
    close(pipe_fd[WRITE_PORT]);

    if (0 == pid) {

        cout << " child ctrl-C handling" << endl;
        _exit(0);

    } else {

        cout << " parent ctrl-C handling" << endl;

        /* enable buffering */
        tcgetattr(STDIN_FILENO, &tio);
        tio.c_lflag |= ICANON;
        tio.c_lflag |= ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);

        _exit(0);
    }
}


void print_help()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application opens and utilizes an unnamed pipe between parent and child process" << endl;

    cout << "  <up key> - increment value" << endl;
    cout << "  <up key> - decrement value" << endl;

    cout << "  h - print this menu" << endl;
    cout << "  q - quit" << endl;

    cout << endl;
    cout << "*********************************************************************"  << endl;
}


int main() {

    if (-1 != pipe(pipe_fd)) {

        /* fork child process */
        if (-1 != (pid = fork())) {

            signal(SIGINT, ctrl_c_handler);

            if (0 == pid) {

                /* child process reads on this pipe */
                close(pipe_fd[WRITE_PORT]);

                /* read until end of byte stream */
                char buf;
                while (read(pipe_fd[READ_PORT], &buf, 1) > 0)
                    cout << buf;

                cout << "child exit" << endl;

                /* close the read port */
                close(pipe_fd[READ_PORT]);
                /* exit and notify parent  */
                _exit(0);
            
            } else {

                int ch;
                static int val{0};
                char buffer[512];

                /* parent process write on this pipe*/
                close(pipe_fd[READ_PORT]);

                print_help();

                // disable buffering
                tcgetattr(STDIN_FILENO, &tio);
                tio.c_lflag &= ~ICANON;
                tio.c_lflag &= ~ECHO;
                tcsetattr(STDIN_FILENO, TCSANOW, &tio);

                signal(SIGINT, ctrl_c_handler);

                while (parent_running && 'q' != (ch = getchar())) {

                    switch (ch) {

                        case 'h':
                            print_help();
                            break;

                        case 27:
                            // up/down arrow
                            if (91 == getchar()) {

                                // if up or down key, modify val and write to pipe
                                int key_val = getchar();
                                if (65 == key_val) {
                                    val++;
                                } else if (66 == key_val) {
                                    val--;
                                } else {
                                    break;
                                }

                                //* release reader */
                                if (val == 10) parent_running = false;

                                if (parent_running) {

                                    int num_chars;
                                    if (0 <= (num_chars = sprintf(buffer, "PID: %d, val: %d\n", pid, val))) {

                                        /* pipe write */
                                        write(pipe_fd[WRITE_PORT], buffer, strlen(buffer));

                                    } else {
                                        cout << "sprintf() failure : " << strerror(errno) << endl;
                                    }

                                } else {

                                    /* generate eof for child process */
                                    close(pipe_fd[WRITE_PORT]);
                                }
                            }
                            break;

                        } // end switch

                } // end while

                /* wait for child _exit */
                wait(NULL);
                /* enable buffering */
                tcgetattr(STDIN_FILENO, &tio);
                tio.c_lflag |= ICANON;
                tio.c_lflag |= ECHO;
                tcsetattr(STDIN_FILENO, TCSANOW, &tio);

                exit(0);

            } // end parent pid

        } else {
            cout << "fork() errno: " << strerror(errno) << endl;
            return -1;
        }
    } else {
        cout << "pipe() errno: " << strerror(errno) << endl;
        return -1;
    }

    return 0;
}
