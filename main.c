#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include "ipc.h"

#define MAX_KASY 10

int main() {
    key_t shm_key = create_key(".", 'm');
    int shm_id = create_shared_memory(shm_key, (MAX_KASY + 1) * sizeof(int), 0600);
    int *shm_ptr = (int *)attach_shared_memory(shm_id, 0);
    shm_ptr[0] = 0;

    key_t sem_key = create_key(".", 's');
    int sem_id = create_semaphore(sem_key, MAX_KASY, 0600);
    
    for (int i = 0; i < 2; i++) {
        set_semaphore(sem_id, i, 1);
    }
    
    for (int i = 2; i < MAX_KASY; i++) {
        set_semaphore(sem_id, i, 0);
    }
    
    for (int i = 0; i < MAX_KASY; i++) {
        key_t msg_key = create_key(".", 'q' + i);
        create_message_queue(msg_key, 0600);
    }
    printf("main\n");

    if (fork() == 0) {
        execl("./kierownik_kasjerow", "kierownik_kasjerow", (char *)NULL);
        exit(1);
    }

    if (fork() == 0) {
        execl("./strazak", "strazak", (char *)NULL);
        exit(1);
    }

    while (1) {
        if (fork() == 0) {
            execl("./klient", "klient", (char *)NULL);
            exit(1);
        }
        sleep(1);
    }

    //sleep(30);
    detach_shared_memory(shm_ptr);
    remove_shared_memory(shm_id);
    remove_semaphore(sem_id);
    return 0;
}

