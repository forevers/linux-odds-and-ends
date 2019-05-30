#pragma once

#define BACKING_FILE "/shmem_backing"

//* show up on /dev/shm/ or similar directory */
#define INTER_PROC_SEM "inter_proc_sem"

enum State {
    STATE_0,
    STATE_1,
    STATE_2,
};

struct SharedData512 {
    pid_t pid;
    State state;
    char buffer[512];
};