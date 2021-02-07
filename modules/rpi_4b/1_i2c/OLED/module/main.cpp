// native compiler options
// export TOOL_PREFIX=/mnt/data/projects/rpi/clones/tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi
// export CXX=$TOOL_PREFIX-g++
// export AR=$TOOL_PREFIX-ar
// export RANLIB=$TOOL_PREFIX-ranlib
// export CC=$TOOL_PREFIX-gcc
// export LD=$TOOL_PREFIX-ld
// export CCFLAGS="-march=armv4"
// g++ -g -O0 -std=c++17 -ggdb -lpthread -o i2c_oled_test main.cpp
// rpi@192.168.1.15
// scp i2c_oled_test pi@192.168.1.15:/home/pi/projects/oled/app

#include <climits>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "i2c_oled_global.h"

using namespace::std;

class ButtonProcessing
{
public:

    bool oled_enabled_;
    bool oled_filled_;
    bool oled_rect_filled_;

    ButtonProcessing(std::string name, int fd, uint8_t x_cur = 0, uint8_t y_cur = 0) :
        thread_(&ButtonProcessing::Run, this, name),
        name_(name),
        fd_(fd),
        ep_fd_(-1),
        epoll_initialized_(false),
        enabled_(true),
        oled_enabled_(true),
        oled_filled_(false),
        oled_rect_filled_(false),
        x_cur_(x_cur),
        y_cur_(y_cur)
    {
        cout<<"ButtonProcessing() constructor, name : "<<name_<<endl;
    }

    ~ButtonProcessing()
    {
        cout<<"ButtonProcessing() destructor, name : "<<name_<<endl;

        /* thread exit */
        enabled_ = false;

        // /* take driver out of toggle mode */
        // ioctl(fd_, CMD_RELEASE_POLL, 0);

        thread_.join();

        /* remove event from file descriptor */
        if (-1 != ep_fd_) {
            if (0 > epoll_ctl(ep_fd_, EPOLL_CTL_DEL, fd_, NULL)) {
                cout<<"epoll_ctl(EPOLL_CTL_DEL failure : "<<strerror(errno)<<endl;
            }
        }
    }

    // Move Constructor
    ButtonProcessing(ButtonProcessing && obj) = delete;
    // Move Assignment Operator
    ButtonProcessing & operator=(ButtonProcessing && obj) = delete;
    /* unsupported operations */
    /* no copy constructor */
    ButtonProcessing(const ButtonProcessing&) = delete;
    /* no Assignment operator */
    ButtonProcessing& operator=(const ButtonProcessing&) = delete;

    uint8_t x_cur_;
    uint8_t y_cur_;

private:

    // PollMode poll_mode_;

    /* init the event poll epoll file descriptor */
    int EpollInit(int epoll_size_approximation)
    {
        int retval = 0;

        if (!epoll_initialized_) {
            if (-1 != (retval = epoll_create(epoll_size_approximation))) {
                ep_fd_ = retval;
                retval = 0;
                epoll_event event;
                event.data.fd = fd_;
                event.events = EPOLLIN;
                if (0 != (retval = epoll_ctl(ep_fd_, EPOLL_CTL_ADD, fd_, &event))) {
                    cout<<"epoll_create() failure : "<<strerror(errno)<<endl;
                }
            } else {
                cout<<"epoll_create() failure : "<<strerror(errno)<<endl;
            }
            epoll_initialized_ = true;
        }

        return retval;
    }

    void Run(std::string command)
    {
        cout<<"DutyCycle::Run() entry"<<endl;

        int retval;

        while (enabled_) {

            sleep(1);

            // two events
            int max_number_events = 2;
            int num_events_received;
            struct epoll_event epoll_events[max_number_events];

            /* estimate number of fd's as 2 */
            EpollInit(2);

            // blocking epoll call
            if (0 < (num_events_received = epoll_wait(ep_fd_, epoll_events, max_number_events, -1))) {

                for (int i = 0; i < num_events_received; i++) {

                    cout<<"event : "<<epoll_events[i].events<<" on fd = "<<epoll_events[i].data.fd<<endl;

                    if (fd_ == epoll_events[i].data.fd) {
                        /* capture event bulk data */
                        struct EventBulkData event_bulk_data;
                        while (sizeof(struct EventBulkData) == read(fd_, &event_bulk_data, sizeof(struct EventBulkData))) {
                            uint64_t event_number = event_bulk_data.capture_event.event;
                            cout<<"event number : "<<event_number<<endl;
                        }
                    }
                }
            } else {
                cout<<"epoll_wait() failure : "<<strerror(num_events_received)<<endl;
            }
        }

        cout<<"thread "<<name_<<" exit"<<endl;
    }

    std::thread thread_;
    std::string name_;

    /* file descriptor to release any blocking poll() epoll() or select() calls */
    int fd_;
    /* epoll file descriptor */
    int ep_fd_;
    bool epoll_initialized_;

    /* thread loop control */
    bool enabled_;
};


void print_key_processing()
{
    cout<<endl<<"****************************** Main Menu ******************************"<<endl;
    cout<<endl;
    cout<<" This application demonstrates a ess_canonical_module system calls"<<endl;
    cout<<" Select one of the following modes using the keyboard"<<endl;
    cout<<" All modes allow value iteration using the up/down keys"<<endl;
    cout<<endl;

    cout<<"  q - quit"<<endl;
    cout<<"  e - toggle oled enable"<<endl;
    cout<<"  f - toggle fill frame buffer"<<endl;
    cout<<"  l - horizontal and vertical lines from pixel location to left/top edge"<<endl;
    cout<<"  r - toggle rectangle from pixel location to left/top edge"<<endl;
    cout<<"  <up key> - move pixel up"<<endl;
    cout<<"  <down key> - move pixel down"<<endl;
    cout<<"  <left key> - move pixel left"<<endl;
    cout<<"  <right key> - move pixel right"<<endl;
    cout<<"  h - Print this menu"<<endl;
    cout<<endl;
    cout<<"*********************************************************************" <<endl;
}


static void process_key_entry(int fd)
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

    shared_ptr<ButtonProcessing> button_processing = std::make_shared<ButtonProcessing>("test", fd);

    print_key_processing();

    // disable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    while ('q' != (ch = getchar())) {
        switch (ch) {

            case 'h': {
                print_key_processing();
                break;
            }

            case 'e': {
                if (button_processing->oled_enabled_) {
                    cout<<"disable oled"<<endl;
                    button_processing->oled_enabled_ = false;
                    ioctl(fd,IOCTL_DISABLE);
                } else {
                    cout<<"enable oled"<<endl;
                    button_processing->oled_enabled_ = true;
                    ioctl(fd, IOCTL_ENABLE);
                }
                break;
            }

            case 'f': {
                if (button_processing->oled_filled_) {
                    cout<<"blank oled"<<endl;
                    button_processing->oled_filled_ = false;
                    ioctl(fd, IOCTL_CLEAR_BUFFER);
                } else {
                    cout<<"fill oled"<<endl;
                    button_processing->oled_filled_ = true;
                    ioctl(fd, IOCTL_FILL_BUFFER);
                }
                break;
            }

            case 'l': {
                /* lines from cur position to left and top edges */
                uint8_t cmd_sequence[4];
                cmd_sequence[0] = CMD_H_LINE;
                cmd_sequence[1] = 0;
                cmd_sequence[2] = button_processing->y_cur_;
                cmd_sequence[3] = button_processing->x_cur_;
                write(fd, cmd_sequence, sizeof(cmd_sequence));
                cmd_sequence[0] = CMD_V_LINE;
                cmd_sequence[1] = button_processing->x_cur_;
                cmd_sequence[2] = 0;
                cmd_sequence[3] = button_processing->y_cur_;
                write(fd, cmd_sequence, sizeof(cmd_sequence));
                break;
            }

            case 'r': {
                uint8_t cmd_sequence[5];
                /* rectangle from cur position to uppper left corner */
                if (button_processing->oled_rect_filled_) {
                    cout<<"oled rectangle blanked"<<endl;
                    button_processing->oled_rect_filled_ = false;
                    cmd_sequence[0] = CMD_RECT_CLEAR;
                    cmd_sequence[1] = 0;
                    cmd_sequence[2] = 0;
                    cmd_sequence[3] = button_processing->x_cur_;
                    cmd_sequence[4] = button_processing->y_cur_;
                    write(fd, cmd_sequence, sizeof(cmd_sequence));
                } else {
                    cout<<"oled retangle filled"<<endl;
                    button_processing->oled_rect_filled_ = true;
                    cmd_sequence[0] = CMD_RECT_FILL;
                    cmd_sequence[1] = 0;
                    cmd_sequence[2] = 0;
                    cmd_sequence[3] = button_processing->x_cur_;
                    cmd_sequence[4] = button_processing->y_cur_;
                    write(fd, cmd_sequence, sizeof(cmd_sequence));
                }
                break;
            }

            case 27: {

                if (91 == getchar()) {
                    int key_val = getchar();
                    if (66 == key_val) {
                        // down arrow
                        if (button_processing->y_cur_ < 63) {
                            uint8_t cmd_sequence[3];
                            button_processing->y_cur_++;
                            cmd_sequence[0] = CMD_SET_PIXEL;
                            cmd_sequence[1] = button_processing->x_cur_;
                            cmd_sequence[2] = button_processing->y_cur_;
                            int bytes = write(fd, cmd_sequence, 3);
                        }
                    } else if (65 == key_val) {
                        /* up arrow */
                        if (button_processing->x_cur_ > 0) {
                            uint8_t cmd_sequence[3];
                            button_processing->y_cur_--;
                            cmd_sequence[0] = CMD_SET_PIXEL;
                            cmd_sequence[1] = button_processing->x_cur_;
                            cmd_sequence[2] = button_processing->y_cur_;
                            int bytes = write(fd, cmd_sequence, 3);
                        }
                    } else if (67 == key_val) {
                        /* right arrow */
                        if (button_processing->x_cur_ < 127) {
                            uint8_t cmd_sequence[3];
                            button_processing->x_cur_++;
                            cmd_sequence[0] = CMD_SET_PIXEL;
                            cmd_sequence[1] = button_processing->x_cur_;
                            cmd_sequence[2] = button_processing->y_cur_;
                            int bytes = write(fd, cmd_sequence, 3);
                        }
                    } else if (68 == key_val) {
                        /* left arrow */
                        if (button_processing->x_cur_ > 0) {
                            uint8_t cmd_sequence[3];
                            button_processing->x_cur_--;
                            cmd_sequence[0] = CMD_SET_PIXEL;
                            cmd_sequence[1] = button_processing->x_cur_;
                            cmd_sequence[2] = button_processing->y_cur_;
                            int bytes = write(fd, cmd_sequence, 3);
                        }
                    } else {
                        break;
                    }
                }
                break;
            }

            default:
                break;

        } // end switch ch
    } // end while

    // enable buffering
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON;
    t.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    /* set or clear ioctl will take driver gpio polling */
    ioctl(fd, IOCTL_RELEASE_POLL);

    /* destroy threaded duty cycle object */
    button_processing = nullptr;
}


int main() 
{
    int fd;

    /* open module handle*/
    if (-1 != (fd = open("/dev/ess-oled", O_RDWR))) {

        cout<<"open() success : "<<endl;

        process_key_entry(fd);

        /* close module handle */
        close(fd);

    } else {
        cout<<"open() failure : "<<strerror(errno)<<endl;
        return -1;
    }

    cout<<"exit"<<endl;

    return 0;
}
