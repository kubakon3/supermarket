#include <signal.h>
#include "ipc.h"

#define MAX_KASY 10

key_t shm_key, sem_key, msg_key;
int shm_id, sem_id, msg_id;
int *liczba_klientow;

// Inicjalizacja połączenia z IPC
void inicjalizuj_ipc() {
    shm_key = create_key(".", 'a');
    sem_key = create_key(".", 'b');
    msg_key = create_key(".", 'c');
    
    shm_id = create_shared_memory(shm_key, sizeof(int), 0600);
    liczba_klientow = (int *)attach_shared_memory(shm_id, 0);

    sem_id = create_semaphore(sem_key, 1, 0600);

    msg_id =  create_message_queue(msg_key, 0600);
}

// Wyslanie sygnalu pozaru
void oglos_pozar() {
    printf("STRAZAK: POZAR!\n");
    kill(0, SIGUSR1);
    
    msgctl(msg_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("STRAŻAK: SKLEP ZAMKNIĘTY!\n");
    exit(0);
}

int main() {
    inicjalizuj_ipc();
    printf("Strazak: nacisnij enter aby wywolac pozar.\n");
    getchar();
    oglos_pozar();
    return 0;
}

