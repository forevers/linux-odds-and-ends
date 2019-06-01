#pragma once

#define BACKING_FILE "./named_pipe"

// //* show up on /dev/shm/ or similar directory */
// #define INTER_PROC_SEM_READ "inter_proc_sem_read"
// #define INTER_PROC_SEM_WRITE "inter_proc_sem_write"


// enum State {
//     STATE_0,
//     STATE_1,
//     STATE_2,
// };

// struct SharedData512 {
//     pid_t pid;
//     State state;
//     char buffer[512];
// };