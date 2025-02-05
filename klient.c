#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "ipc.h"

struct msgbuf {
    long mtype;
    int klient_id;
};

int main() {
    key_t shm_key = create_key(".", 'm');
    int shm_id = create_shared_memory(shm_key, 0, 0600);
    int *shm_ptr = (int *)attach_shared_memory(shm_id, 0);

    shm_ptr[0]++;
    int min_kolejka = 1, min_dlugosc = shm_ptr[11];
    for (int i = 2; i <= 10; i++) {
        if (shm_ptr[10 + i] < min_dlugosc) {
            min_dlugosc = shm_ptr[10 + i];
            min_kolejka = i;
        }
    }
    printf("klient\n");

    key_t msg_key = create_key(".", 'q' + min_kolejka - 1);
    int msg_id = create_message_queue(msg_key, 0600);
    printf("klient %d robi zakupy\n", getpid());
    sleep(rand() % 3 + 1);
    struct msgbuf klient = {1, getpid()};
    send_message(msg_id, &klient, sizeof(int), 0);
    receive_message(msg_id, &klient, sizeof(int), getpid(), 0);
    printf("klient %d wychodzi ze sklepu\n", getpid());

    shm_ptr[0]--;
    detach_shared_memory(shm_ptr);
    exit(0);
}
