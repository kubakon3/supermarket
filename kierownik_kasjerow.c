#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ipc.h"

#define MAX_KASY 10
#define MIN_KASY 2
#define KLIENT_NA_KASE 10
#define MAX_KLIENCI 100

typedef struct {
    long mtype;
    int klient_id;
} komunikat;

key_t shm_key, sem_key, msg_key;
int shm_id, sem_id, msg_id;
int *liczba_klientow;
pid_t kasjerzy[MAX_KASY];

// Inicjalizacja pamieci dzielonej i semaforow
void inicjalizuj_ipc() {
    shm_key = create_key(".", 'a');
    sem_key = create_key(".", 'b');
    msg_key = create_key(".", 'c');
    
    shm_id = create_shared_memory(shm_key, sizeof(int), 0600);
    liczba_klientow = (int *)attach_shared_memory(shm_id, 0);

    sem_id = create_semaphore(sem_key, 1, 0600);

    msg_id =  create_message_queue(msg_key, 0600);
}

// Tworzenie kasjerów
void stworz_kasjerow(int liczba_kas) {
    for (int i = 0; i < liczba_kas; i++) {
        if ((kasjerzy[i] = fork()) == 0) {
            execl("./kasjer", "kasjer", NULL);
            perror("Blad uruchamiania kasjera");
            exit(1);
        }
    }
}

// Zarzadzanie kasami
void monitoruj_kasy() {
    int liczba_kas = MIN_KASY;
    stworz_kasjerow(liczba_kas);
    while (1) {
        sleep(5);
        if (*liczba_klientow / KLIENT_NA_KASE + 1 > liczba_kas && liczba_kas < MAX_KASY) {
            liczba_kas++;
            stworz_kasjerow(1);
        } else if (*liczba_klientow < KLIENT_NA_KASE * (liczba_kas - 1) && liczba_kas > MIN_KASY) {
            kill(kasjerzy[--liczba_kas], SIGTERM);
        }
    }
}

// Zamkniecie kas na sygnał pozaru
void zamknij_kasy(int sig) {
    for (int i = 0; i < MAX_KASY; i++) {
        if (kasjerzy[i] > 0) {
            kill(kasjerzy[i], SIGTERM);
        }
    }
    remove_message_queue(msg_id);
    remove_semaphore(sem_id);
    remove_shared_memory(shm_id);
    exit(0);
}

int main() {
    signal(SIGUSR1, zamknij_kasy);
    inicjalizuj_ipc();
    monitoruj_kasy();
    return 0;
}

