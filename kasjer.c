#include <signal.h>
#include <string.h>
#include "ipc.h"

#define MAX_KASY 10
#define MSG_SIZE 128


struct msgbuf {
    long mtype;
    char mtext[MSG_SIZE];
};

key_t shm_key, sem_key, msg_key, shm_key_2;
int shm_id, sem_id, msg_id, shm_id_2;
int *kolejki;
int numer_kasy;

// Inicjalizacja połączenia z IPC
void inicjalizuj_ipc() {
    //shm_key = create_key(".", 'a');
    shm_key_2 = create_key(".", 'd');
    sem_key = create_key(".", 'b');
    msg_key = create_key(".", 'c');
    
    //shm_id = create_shared_memory(shm_key, sizeof(int), 0600);
    //liczba_klientow = (int *)attach_shared_memory(shm_id, 0);
    
    shm_id_2 = create_shared_memory(shm_key_2, MAX_KASY * sizeof(int), 0600);
    kolejki = (int *)attach_shared_memory(shm_id_2, 0);

    sem_id = create_semaphore(sem_key, 1, 0600);

    msg_id =  create_message_queue(msg_key, 0600);
}


// Obsługa klientów
void obsluga_klientow() {
    //dodać logikę obsługi klienta
}

// Obsługa sygnału zamknięcia kasy
void sygnal_zamkniecia(int sig) {
    printf("Kasjer %d: Otrzymano sygnał zamknięcia!\n", numer_kasy);
    msgctl(shm_id_2, IPC_RMID, NULL);
    exit(0);
}

int main() {
    srand(getpid());
    inicjalizuj_ipc();
    signal(SIGUSR1, sygnal_zamkniecia);
    //obsluga_klientow();
    return 0;
}

