// native compiler options
// g++ -g -O0 -std=c++17 -ggdb -lpthread -o i2c_oled_test main.cpp

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

    void PixelDown()
    {
        if (y_cur_ < 63) {
            uint8_t cmd_sequence[3];
            y_cur_++;
            cmd_sequence[0] = CMD_SET_PIXEL;
            cmd_sequence[1] = x_cur_;
            cmd_sequence[2] = y_cur_;
            write(fd_, cmd_sequence, 3);
        }
    }
    
    void PixelUp()
    {
        if (x_cur_ > 0) {
            uint8_t cmd_sequence[3];
            y_cur_--;
            cmd_sequence[0] = CMD_SET_PIXEL;
            cmd_sequence[1] = x_cur_;
            cmd_sequence[2] = y_cur_;
            write(fd_, cmd_sequence, 3);
        }
    }
        
    void PixelRight()
    {               
        if (x_cur_ < 127) {
            uint8_t cmd_sequence[3];
            x_cur_++;
            cmd_sequence[0] = CMD_SET_PIXEL;
            cmd_sequence[1] = x_cur_;
            cmd_sequence[2] = y_cur_;
            write(fd_, cmd_sequence, 3);
        }
    }
    
    void PixelLeft()
    {
        if (x_cur_ > 0) {
            uint8_t cmd_sequence[3];
            x_cur_--;
            cmd_sequence[0] = CMD_SET_PIXEL;
            cmd_sequence[1] = x_cur_;
            cmd_sequence[2] = y_cur_;
            write(fd_, cmd_sequence, 3);
        }
    }
    
    void ToggleFill()
    {
        if (oled_filled_) {
            cout<<"blank oled"<<endl;
            oled_filled_ = false;
            ioctl(fd_, IOCTL_CLEAR_BUFFER);
        } else {
            cout<<"fill oled"<<endl;
            oled_filled_ = true;
            ioctl(fd_, IOCTL_FILL_BUFFER);
        }
    }  
    
    /* lines from cur position to left and top edges */
    void LineToTopLeft()
    {
        uint8_t cmd_sequence[4];
        cmd_sequence[0] = CMD_H_LINE;
        cmd_sequence[1] = 0;
        cmd_sequence[2] = y_cur_;
        cmd_sequence[3] = x_cur_;
        write(fd_, cmd_sequence, sizeof(cmd_sequence));
        cmd_sequence[0] = CMD_V_LINE;
        cmd_sequence[1] = x_cur_;
        cmd_sequence[2] = 0;
        cmd_sequence[3] = y_cur_;
        write(fd_, cmd_sequence, sizeof(cmd_sequence));
    }
    
    /* rectangle from cur position to uppper left corner */
    void RectToTopLeft()
    {
        uint8_t cmd_sequence[5];

        if (oled_rect_filled_) {
            cout<<"oled rectangle blanked"<<endl;
            oled_rect_filled_ = false;
            cmd_sequence[0] = CMD_RECT_CLEAR;
            cmd_sequence[1] = 0;
            cmd_sequence[2] = 0;
            cmd_sequence[3] = x_cur_;
            cmd_sequence[4] = y_cur_;
            write(fd_, cmd_sequence, sizeof(cmd_sequence));
        } else {
            cout<<"oled retangle filled"<<endl;
            oled_rect_filled_ = true;
            cmd_sequence[0] = CMD_RECT_FILL;
            cmd_sequence[1] = 0;
            cmd_sequence[2] = 0;
            cmd_sequence[3] = x_cur_;
            cmd_sequence[4] = y_cur_;
            write(fd_, cmd_sequence, sizeof(cmd_sequence));
        }
    }
                
private:

    uint8_t x_cur_;
    uint8_t y_cur_;


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
        cout<<"Run() entry"<<endl;

        int retval;

        while (enabled_) {

            //sleep(1);

            // two events
            int max_number_events = 2;
            int num_events_received;
            struct epoll_event epoll_events[max_number_events];

            /* estimate number of fd's as 2 */
            EpollInit(2);

            // blocking epoll call
            if (0 < (num_events_received = epoll_wait(ep_fd_, epoll_events, max_number_events, -1))) {
                for (int i = 0; i < num_events_received; i++) {

                    //cout<<"event : "<<epoll_events[i].events<<" on fd = "<<epoll_events[i].data.fd<<endl;

                    if (fd_ == epoll_events[i].data.fd) {
                        bool read_ok = true;
                        /* capture event bulk data */
                        struct EventBulkData event_bulk_data;
                        ssize_t remaining = sizeof(struct EventBulkData); 
                        uint8_t* buffer = reinterpret_cast<uint8_t*>(&event_bulk_data); 
                        //while (sizeof(struct EventBulkData) == read(fd_, &event_bulk_data, sizeof(struct EventBulkData))) {
                        //    uint64_t button_number = event_bulk_data.capture_event.button_num;
                        //    uint64_t event_number = event_bulk_data.capture_event.event;
                        //    cout<<"button: "<<button_number<<", event number : "<<event_number<<endl;
                        //}
                        ssize_t ret;
                        while (remaining != 0 && ((ret = read(fd_, buffer, remaining)) != 0)) {
                            if (ret > 0) {
                                remaining -= ret;
                                buffer += ret;
                            } else {
                                if (errno == EINTR) continue;
                                cout<<"read error: "<<ret<<endl;
                                read_ok = false;
                                break;
                            }
                        }
                        if (read_ok) {
                            uint64_t button_number = event_bulk_data.capture_event.button_num;
                            uint64_t event_number = event_bulk_data.capture_event.event;
                            cout<<"button: "<<button_number<<", event number : "<<event_number<<endl;
                            
                            switch(button_number) {
                                case(BUTTON_5):
                                    LineToTopLeft();
                                    break;
                                case(BUTTON_6):
                                    RectToTopLeft();
                                    break;
                                case(ROCKER_D):
                                    ToggleFill();
                                case(ROCKER_N):
                                    PixelUp();
                                    break;
                                case(ROCKER_S):
                                    PixelDown();
                                    break;
                                case(ROCKER_E):
                                    PixelRight();
                                    break;
                                case(ROCKER_W):
                                    PixelLeft();
                                    break;
                                default:
                                    break;
                            }
                            
                            if (button_number == ROCKER_S) PixelDown();
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

    /* file descriptor to release blocking epoll() calls */
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
                button_processing->ToggleFill();
                break;
            }

            case 'l': {
                button_processing->LineToTopLeft();
                break;
            }

            case 'r': {
                button_processing->RectToTopLeft();
                break;
            }

            case 27: {

                if (91 == getchar()) {
                    int key_val = getchar();
                    if (66 == key_val) {
                        // down arrow
                        button_processing->PixelDown();
                    } else if (65 == key_val) {
                        /* up arrow */
                        button_processing->PixelUp();
                    } else if (67 == key_val) {
                        /* right arrow */
                        button_processing->PixelRight();
                    } else if (68 == key_val) {
                        /* left arrow */
                        button_processing->PixelLeft();
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
