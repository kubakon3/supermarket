#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "ipc.h"

struct msgbuf {
    long mtype;
    int klient_id;
};

int sem_id, msg_id;
void zakoncz(int sig) { 
    exit(0); 
}

int main() {
    key_t msg_key = create_key(".", 'q');
    msg_id = create_message_queue(msg_key, 0600);
    key_t sem_key = create_key(".", 's');
    sem_id = create_semaphore(sem_key, 10, 0600);
    signal(SIGTERM, zakoncz);
    printf("kasjer\n");

    while (1) {
        struct msgbuf klient;
        receive_message(msg_id, &klient, sizeof(int), 1, 0);
        printf("kasjer %d obsluguje klienta: %d\n", getpid(), klient.klient_id);
        sleep(rand() % 3 + 1);
        klient.mtype = klient.klient_id;
        send_message(msg_id, &klient, sizeof(int), 0);
    }
    exit(0);
}
