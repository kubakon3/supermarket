#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>
#include "ipc.h"

#define MAX_KASY 10
#define K 10
#define MAX_KLIENCI 100

struct sembuf sem_open = {0, 1, 0};
struct sembuf sem_close = {0, -1, 0};

int shm_id, sem_id, msg_id[MAX_KASY];
int *shm_ptr;

void zamknij_kasy() {
    for (int i = 0; i < MAX_KASY; i++) {
        semctl(sem_id, i, SETVAL, 0);
        kill(shm_ptr[i + 1], SIGTERM);
        for (int i = 0; i < MAX_KASY; i++) {
            remove_message_queue(i);
        }
    }
}

void sygnal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        zamknij_kasy();
        shmdt(shm_ptr);
        shmctl(shm_id, IPC_RMID, NULL);
        semctl(sem_id, 0, IPC_RMID);
        exit(0);
    }
}

int main() {
    key_t shm_key = create_key(".", 'm');
    shm_id = create_shared_memory(shm_key, (MAX_KASY + 1) * sizeof(int), 0600);
    shm_ptr = (int *)attach_shared_memory(shm_id, 0);
    //shm_ptr[0] = 0;

    key_t sem_key = create_key(".", 's');
    sem_id = create_semaphore(sem_key, MAX_KASY, 0600);
    for (int i = 0; i < 2; i++) {
        set_semaphore(sem_id, i, 1);
    }
    for (int i = 2; i < MAX_KASY; i++) {
        set_semaphore(sem_id, i, 0);
    }
    printf("kierownik\n");

    for (int i = 0; i < MAX_KASY; i++) {
        key_t msg_key = create_key(".", 'q' + i);
        msg_id[i] = create_message_queue(msg_key, 0600);
        if (fork() == 0) {
            execl("./kasjer", "kasjer", (char *)NULL);
            exit(1);
        }
    }

    signal(SIGINT, sygnal_handler);
    signal(SIGTERM, sygnal_handler);

    while (1) {
        sleep(2);
        int klienci = shm_ptr[0];
        int otwarte_kasy = 2;
        for (int i = 2; i < MAX_KASY; i++) {
            if (klienci > K * otwarte_kasy) {
                set_semaphore(sem_id, i, 1);
                otwarte_kasy++;
            } else if (klienci < K * (otwarte_kasy - 1)) {
                set_semaphore(sem_id, i, 0);
                otwarte_kasy--;
            }
        }
    }
    for (int i = 0; i < MAX_KASY; i++) {
        remove_message_queue(i);
    }
    exit(0);
}

