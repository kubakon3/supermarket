#include <signal.h>
#include <time.h>
#include "ipc.h"

#define KLIENT_NA_KASE 10
#define MAX_KLIENCI 100
#define MAX_KASY 10

struct msgbuf {
    long mtype;
    char mtext[128];
};

key_t shm_key, sem_key, msg_key;
int shm_id, sem_id, msg_id;
int *liczba_klientow;

// Inicjalizacja mechanizmów IPC
void inicjalizuj_ipc() {
    shm_key = create_key(".", 'a');
    sem_key = create_key(".", 'b');
    msg_key = create_key(".", 'c');
    
    shm_id = create_shared_memory(shm_key, sizeof(int), 0600);
    liczba_klientow = (int *)attach_shared_memory(shm_id, 0);

    sem_id = create_semaphore(sem_key, 1, 0600);

    msg_id =  create_message_queue(msg_key, 0600);
}

// Wybor kasy
int wybierz_kase() {
    return rand() % MAX_KASY + 1; 
}

// Dodanie do kolejki
void dolacz_do_kolejki(int kasa_id) {
    printf("kient %d wszedl do sklepu", getpid());
    //dodać logikę wybierania kasy
}

// Wyjscie ze sklepu
void opusc_sklep() {
    (*liczba_klientow)--;
    printf("Klient %d opuscil sklep\n", getpid());
    exit(0);
}

int main() {
    signal(SIGUSR1, opusc_sklep);
    srand(time(NULL));
    inicjalizuj_ipc();
    (*liczba_klientow)++;
    
    int kasa_id = wybierz_kase();
    dolacz_do_kolejki(kasa_id);
    
    opusc_sklep();
    return 0;
}

