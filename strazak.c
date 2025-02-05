#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include "ipc.h"

int shm_id, sem_id, *shm_ptr;

void alarm_handler(int sig) {
    printf("ALARM! Ewakuacja klientów!\n");
    //shm_ptr[0] = 0;
    for (int i = 0; i < 10; i++) {
        set_semaphore(sem_id, i, 0);
    }
    detach_shared_memory(shm_ptr);
    remove_shared_memory(shm_id);
    remove_semaphore(sem_id);
    exit(0);
}

int main() {
    key_t shm_key = create_key(".", 'm');
    shm_id = create_shared_memory(shm_key, 0, 0600);
    shm_ptr = (int *)attach_shared_memory(shm_id, 0);

    key_t sem_key = create_key(".", 's');
    sem_id = create_semaphore(sem_key, 10, 0600);
    printf("strazak\n");

    signal(SIGINT, alarm_handler);
    while (1) pause();
}
