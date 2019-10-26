// compiler options
// $ g++ -g -O0 -std=c++14 -ggdb -lpthread -o gpio_test main.cpp
// $ clang++ -g -O0 -std=c++14 -lpthread -o gpio_test main.cpp

#include <climits>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "gpio_irq_global.h"

using namespace::std;

class DutyCycle
{
public:
    DutyCycle(std::string name, int fd) :
        thread_(&DutyCycle::Run, this, name),
        name_(name),
        fd_(fd),
        enabled_(true)
    {
        cout << "DutyCycle() constructor, name : " << name_ << endl;
    }

    ~DutyCycle()
    {
        cout << "DutyCycle() destructor, name : " << name_ << endl;

        /* thread exit */
        enabled_ = false;

        /* take driver out of toggle mode */
        ioctl(fd_, ESS_DUTY_CYCLE_GPIO, 0);

        thread_.join();
    }

    // Move Constructor
    DutyCycle(DutyCycle && obj) = delete;
    //Move Assignment Operator
    DutyCycle & operator=(DutyCycle && obj) = delete;
    /* unsupported operations */
    /* no copy constructor */
    DutyCycle(const DutyCycle&) = delete;
    /* no Assignment opeartor */
    DutyCycle& operator=(const DutyCycle&) = delete;

private:

    void Run(std::string command)
    {
        cout << "DutyCycle::Run() entry" << endl;

        fd_set read_fd_set;

        cout << "enabled_ = " << ((enabled_ == true) ? "true" : "false") << endl;
        while (enabled_) {

            int retval;
            FD_ZERO(&read_fd_set);
            FD_SET(fd_, &read_fd_set);

            /* select blocking */
            if (-1 != (retval = select(fd_+1, &read_fd_set, NULL, NULL, NULL))) {

                if (FD_ISSET(fd_, &read_fd_set)) {
                    // cout << "select() released" << endl;

                    /* capture event bulk data */
                    struct EventBulkData event_bulk_data;

                    while (sizeof(struct EventBulkData) == read(fd_, &event_bulk_data, sizeof(struct EventBulkData))) {

                        // TODO event_bulk_data.bulk_data
                        uint64_t event_number = event_bulk_data.capture_event.event;
                        cout << "event number : " << event_number << endl;
                    }
                }
            } else {
                cout << "select() failure : " << strerror(errno) << endl;
            }

        }

        cout << "thread " << name_ << " exit" << endl;
    }

    std::thread thread_;

    std::string name_;

    /* file descriptor to release any blocking poll() epoll() or select() calls */
    int fd_;

    bool enabled_;
};

enum class TestModeE {
    GPIO_TOGGLE_MODE,
    GPIO_BURST_MODE,
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
    cout << "  b - place in burst mode" << endl;
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
    uint8_t short_period_msec = 0x3F;
    uint8_t long_period_msec = 0xFF;
    uint8_t gpio_burst_sequence[] = {
        0x00, short_period_msec,
        0xFF, short_period_msec,
        0x00, short_period_msec,
        0xFF, long_period_msec,
        0x00, short_period_msec};

    shared_ptr<DutyCycle> duty_cycle = nullptr;

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

        case 'b':
            cout << "Burst Mode" << endl;
            if (test_mode != TestModeE::GPIO_BURST_MODE) {

                short_period_msec = 0x3F;
                long_period_msec = 0xFF;

                /* set or clear ioctl will take driver out of toggle mode */
                gpio_burst_sequence[3] = short_period_msec;
                gpio_burst_sequence[7] = long_period_msec;
                int num_gpio_writes = write(ess_fd, gpio_burst_sequence, sizeof(gpio_burst_sequence));
                if (num_gpio_writes >= 0) {
                    cout << "num_gpio_writes : " << num_gpio_writes << endl;
                } else {
                    cout << "write() failure : " << num_gpio_writes << endl;
                }

                /* destroy threaded duty cycle object */
                duty_cycle = nullptr;
                test_mode = TestModeE::GPIO_BURST_MODE;
            }
            break;

        case 't':
            cout << "Toggle Mode" << endl;
            if (test_mode != TestModeE::GPIO_TOGGLE_MODE) {

                /* set or clear ioctl will take driver out of toggle mode */
                ioctl(ess_fd, ESS_CLR_GPIO);

                /* destroy threaded duty cycle object */
                duty_cycle = nullptr;
                test_mode = TestModeE::GPIO_TOGGLE_MODE;
            }
            break;

        case 'd':
            cout << "Duty Cycle Mode" << endl;
            if (test_mode != TestModeE::GPIO_DUTY_CYCLE_MODE) {
                int retval;
                test_mode = TestModeE::GPIO_DUTY_CYCLE_MODE;
                if (0 == (retval = ioctl(ess_fd, ESS_DUTY_CYCLE_GPIO, duty_cycle_msec))) {
                    ioctl(ess_fd, ESS_DUTY_CYCLE_GPIO, duty_cycle_msec);
                    duty_cycle = std::make_shared<DutyCycle>("test", ess_fd);
                } else {
                    cout << "ESS_DUTY_CYCLE_GPIO config error : " << retval << endl;
                }
            }
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

            case TestModeE::GPIO_BURST_MODE:
                int num_gpio_writes;
                if ( (true == advance_mode) && (gpio_burst_sequence[7] < UCHAR_MAX) ) {
                    /* advance high pulse time if not already at max */
                    gpio_burst_sequence[3]++; gpio_burst_sequence[7]++;
                } else if ( (false == advance_mode) && (gpio_burst_sequence[1] > 1) ) {
                    /* reduce high pulse time if not already at min */
                    gpio_burst_sequence[3]--; gpio_burst_sequence[7]--;
                }
                num_gpio_writes = write(ess_fd, gpio_burst_sequence, sizeof(gpio_burst_sequence));
                if (num_gpio_writes >= 0) {
                    cout << "num_gpio_writes : " << num_gpio_writes << endl;
                } else {
                    cout << "write() failure : " << num_gpio_writes << endl;
                }

                break;

            case TestModeE::GPIO_TOGGLE_MODE:
                uint8_t gpio_val;
                ssize_t num_read;
                if ((num_read = read(ess_fd, &gpio_val, 1)) == 1) {
                    if (gpio_val) {
                        cout << "clear GPIO" << endl;
                        ioctl(ess_fd, ESS_CLR_GPIO);
                    } else {
                        cout << "set GPIO" << endl;
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

    /* release resources */
    if (test_mode != TestModeE::GPIO_TOGGLE_MODE) {

        /* set or clear ioctl will take driver out of toggle mode */
        ioctl(ess_fd, ESS_CLR_GPIO);

        /* destroy threaded duty cycle object */
        duty_cycle = nullptr;
        test_mode = TestModeE::GPIO_TOGGLE_MODE;
    }
}


int main() 
{
    int ess_fd;

    /* open module handle*/
    if (-1 != (ess_fd = open("/dev/ess-device-name", O_RDWR))) {

        size_t num_gpio_writes;

        cout << "open() success : " << endl;

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
