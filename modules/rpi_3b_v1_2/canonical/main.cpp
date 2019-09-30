// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -o gpio_test main.cpp
// $ clang++ -g -O0 -std=c++14 -o gpio_test main.cpp

#include <climits>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "gpio_irq_global.h"

using namespace::std;

enum class TestModeE {
    GPIO_TOGGLE_MODE,
    GPIO_DUTY_CYCLE_MODE,
};


void print_key_processing()
{
    cout << endl << "****************************** Main Menu ******************************" << endl;
    cout << endl;
    cout << " This application demonstrates a ess_canonical_module system calls" << endl;
    cout << " Select one of the following modes using the keyboard" << endl;
    cout << " All modes allow value iteration using the up/down keys" << endl;
    cout << endl;

    cout << "  q - quit" << endl;
    cout << "  t - place in toggle mode" << endl;
    cout << "  d - place in duty cycle mode" << endl;
    cout << "  <up key> - increment mode setting" << endl;
    cout << "  <down key> - decrement mode setting" << endl;
    cout << "  h - Print this menu" << endl;
    cout << endl;
    cout << "*********************************************************************"  << endl;
}


static void process_key_entry(int ess_fd)
{
    int ch;
    struct termios t;
    unsigned long duty_cycle_msec = 1000;

    TestModeE test_mode = TestModeE::GPIO_TOGGLE_MODE;

    print_key_processing();

    // disable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    while ('q' != (ch = getchar())) {

        switch (ch) {
        
        case 'h':
            print_key_processing();
            break;
        
        case 't':
            test_mode = TestModeE::GPIO_TOGGLE_MODE;
            break;

        case 'd':
            test_mode = TestModeE::GPIO_DUTY_CYCLE_MODE;
            break;

        case 27:
            bool advance_mode;
            
            // up/down arrow
            if (91 == getchar()) {
                int key_val = getchar();
                if (65 == key_val) {
                    advance_mode = true;
                } else if (66 == key_val) {
                    advance_mode = false;
                } else {
                    break;
                }
            }

            switch(test_mode) {

            case TestModeE::GPIO_TOGGLE_MODE:
                uint8_t gpio_val;
                ssize_t num_read;
                if ((num_read = read(ess_fd, &gpio_val, 1)) == 1) {
                    if (gpio_val) {
                        cout << "ESS_CLR_GPIO" << endl;
                        ioctl(ess_fd, ESS_CLR_GPIO);
                    } else {
                        cout << "ESS_SET_GPIO" << endl;
                        ioctl(ess_fd, ESS_SET_GPIO);
                    }
                } else {
                    cout << " read() failure : " << num_read << endl;
                    break;
                }

                break;

            case TestModeE::GPIO_DUTY_CYCLE_MODE:
                if (advance_mode) {
                    if (duty_cycle_msec < ULONG_MAX) duty_cycle_msec++;
                    cout << " duty_cycle_msec: " << duty_cycle_msec << endl;
                } else {
                    if (duty_cycle_msec > 0) duty_cycle_msec--;
                    cout << " duty_cycle_msec: " << duty_cycle_msec << endl;
                }
                ioctl(ess_fd, ESS_DUTY_CYCLE_GPIO, duty_cycle_msec);

                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    // enable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON;
    t.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}


int main() 
{
    int ess_fd;

    /* open module handle*/
    if (-1 != (ess_fd = open("/dev/ess-device-name", O_RDWR))) {

        size_t num_gpio_writes;

        cout << "open() success : " << endl;

        // gpio write takes array of value / msec delay pairs
        uint8_t gpio_off[] = {0x00, 0xFF};
        if ((num_gpio_writes = write(ess_fd, gpio_off, sizeof(gpio_off))) >= 0) {
            cout << "num_gpio_writes : " << num_gpio_writes << endl;
        } else {
            cout << "write() failure : " << num_gpio_writes << endl;
        }

        uint8_t gpio_on[] = {0xFF, 0xFF};
        if ((num_gpio_writes = write(ess_fd, gpio_on, sizeof(gpio_on))) >= 0) {
            cout << "num_gpio_writes : " << num_gpio_writes << endl;
        } else {
            cout << "write() failure : " << num_gpio_writes << endl;
        }

        uint8_t gpio_sequence[] = {
            0x00, 10,
            0xFF, 20,
            0x00, 30,
            0xFF, 40};
        if ((num_gpio_writes = write(ess_fd, gpio_sequence, sizeof(gpio_sequence))) >= 0) {
            cout << "num_gpio_writes : " << num_gpio_writes << endl;
        } else {
            cout << "write() failure : " << num_gpio_writes << endl;
        }

        ioctl(ess_fd, ESS_SET_GPIO);
        usleep(1000);
        ioctl(ess_fd, ESS_CLR_GPIO);

        process_key_entry(ess_fd);

        /* close module handle */
        close(ess_fd);

    } else {
        cout << "open() failure : " << strerror(errno) << endl;
        return -1;
    }

    cout << "exit" << endl;

    return 0;
}
