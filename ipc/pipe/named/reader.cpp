// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o reader reader.cpp
// $ clang++ -g -O0 -std=c++14 -o reader reader.cpp

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <unistd.h>

#include "named_pipe.h"

using namespace::std;

int main() 
{
    int reader_fd;

    if (-1 != (reader_fd = open(BACKING_FILE, O_RDONLY))) {

        /* read until end of byte stream */
        char buf;
        while (read(reader_fd, &buf, sizeof(buf) > 0))
            cout << buf;

        cout << "child exit" << endl;

        /* close read side of pipe */
        close(reader_fd);
        unlink(BACKING_FILE);

    } else {
        cout << "open() failure : " << strerror(errno) << endl;
        return -1;
    }

    cout << "exit" << endl;

    return 0;
}
