#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "ipc.h"
#include "strazak.h"

extern Sklep *sklep;
extern int semID;

pthread_t kasjerzy[MAX_KASY];
int aktywne_kasy[MAX_KASY] = {0};

// Funkcja wykonywana przez wątek kasjera
void *kasjer(void *arg) {
    srand(time(NULL));
    int kasjer_id = (int)(long)arg;
    pthread_t pid = pthread_self();
    printf("\t\tAktywny kasjer: %d o pid: %lu \n", kasjer_id, pid);
    Komunikat msg;
    while (1) {
        if (msgrcv(kasjer_id, &msg, sizeof(msg) - sizeof(long), kasjer_id, IPC_NOWAIT) != -1) {
            int czas = rand() % 8 + 3;
            printf("Kasjer %d obsługuje klienta %d przez %d sekund\n", kasjer_id, msg.klient_id, czas);
            sleep(czas);
            msg.mtype = msg.klient_id;
            if (msgsnd(kasjer_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("Błąd wysyłania komunikatu od kasjera do klienta");
                break;
            }
        } else if (errno == ENOMSG) {
            sleep(3);
            continue;
        } else {
            perror("Błąd odbierania komunikatu klient od kasjera");
            break;
        }
    }
    pthread_exit(NULL);
}

// Funkcja monitorująca stan kas
void aktualizuj_kasy() {
    if (semaphore_p(semID, 0) == -1) { // Semafor 0 (dostęp do pamięci dzielonej)
        perror("Błąd semaphore_p w aktualizuj_kasy");
        return;
    }

    printf("sprawdzanie liczby kas....\n");
    if (sklep->liczba_klientow / K >= sklep->liczba_kas && sklep->liczba_kas < MAX_KASY) {
        sklep->liczba_kas++;
        if (!aktywne_kasy[sklep->liczba_kas - 1]) {
            pthread_create(&kasjerzy[sklep->liczba_kas - 1], NULL, kasjer, (void *)(long)(sklep->liczba_kas - 1));
            aktywne_kasy[sklep->liczba_kas - 1] = 1;
        }
    } else if (sklep->liczba_klientow < K * (sklep->liczba_kas - 1) && sklep->liczba_kas > 2) {
        sklep->liczba_kas--;
        if (aktywne_kasy[sklep->liczba_kas]) {
            pthread_cancel(kasjerzy[sklep->liczba_kas]);
            aktywne_kasy[sklep->liczba_kas] = 0;
        }
    }
    printf("Aktualna liczba kas: %d\n", sklep->liczba_kas);

    if (semaphore_v(semID, 0) == -1) { // Semafor 0 (dostęp do pamięci dzielonej)
        perror("Błąd semaphore_v w aktualizuj_kasy");
        return;
    }

    printf("koniec sprawdzania liczby kas....\n");
}

int main() {
    // Dołączenie pamięci dzielonej
    sklep = get_shared_memory();
    if (sklep == NULL) {
        exit(1);
    }
    
    semID = semget(SEM_KEY, 2, 0600);
    if (semID == -1) {
        perror("Błąd dołączania semafora");
        shmdt(sklep);
        exit(1);
    }

    // Tworzenie 2 początkowych kas
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&kasjerzy[i], NULL, kasjer, (void *)(long)sklep->kolejki_kas[i]) != 0) {
            perror("Błąd tworzenia wątku kasjera");
            if (shmdt(sklep) == -1) {
                perror("Błąd odłączania segmentu pamięci dzielonej");
                exit(1);
            }
            exit(1);
        }
        aktywne_kasy[i] = 1;
    }

    // Pętla monitorująca stan kas
    while (1) {
        aktualizuj_kasy();
        sleep(3); // Czekaj 1 sekundę przed kolejną aktualizacją
    }

    // Zakończenie programu ()
    for (int i = 0; i < MAX_KASY; i++) {
        if (aktywne_kasy[i]) {
            pthread_cancel(kasjerzy[i]);
        }
    }

    // Odłączenie pamięci współdzielonej
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }

    return 0;
}
