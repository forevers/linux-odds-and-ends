// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o gpio_test main.cpp
// $ clang++ -g -O0 -std=c++14 -o gpio_test main.cpp

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <unistd.h>

#include "gpio_irq_global.h"

using namespace::std;

int main() 
{

    int ess_fd;

    if (-1 != (ess_fd = open("/dev/ess-device-name", O_RDWR))) {

        cout << "open() success : " << endl;

        // gpio write takes array of value / msec delay pairs
        uint8_t gpio_off[] = {0x00, 0xFF};
        size_t num_bytes_written = write(ess_fd, gpio_off, sizeof(gpio_off));
        cout << "num_bytes_written : " << num_bytes_written << endl;

        uint8_t gpio_on[] = {0xFF, 0xFF};
        num_bytes_written = write(ess_fd, gpio_on, sizeof(gpio_on));
        cout << "num_bytes_written : " << num_bytes_written << endl;

        uint8_t gpio_sequence[] = {0x00, 10,   0xFF, 20,   0x00, 30,   0xFF, 40};
        num_bytes_written = write(ess_fd, gpio_sequence, sizeof(gpio_sequence));
        cout << "num_bytes_written : " << num_bytes_written << endl;

        /* close read side of pipe */
        close(ess_fd);

    } else {
        cout << "open() failure : " << strerror(errno) << endl;
        return -1;
    }

    cout << "exit" << endl;

    return 0;
}