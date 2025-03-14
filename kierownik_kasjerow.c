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
extern int aktywne_kasy[MAX_KASY];
/*
void cashier_signal_handler(int sig) {    
    if (semaphore_v(semID, 1) == -1) {
        perror("Błąd zwiększania semafora klientów");
        shmdt(sklep);
        exit(1);
    }
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }
    printf("Klient %d opuścił sklep\n", pid);
    exit(1);
}
*/
// Funkcja wykonywana przez wątek kasjera
void *kasjer(void *arg) {
    srand(time(NULL));
    int index = (int)(long)arg;
    int kasjer_id = get_queue_id(index);
    //pthread_t pid = pthread_self();
    printf("\t\tAktywny kasjer: %d o id: %d \n", index + 1, kasjer_id);
    Komunikat msg;
    while (!check_fire_flag(sklep)) {
        if (msgrcv(kasjer_id, &msg, sizeof(msg) - sizeof(long), kasjer_id, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                // Sprawdzenie, czy kasjer jest nadal aktywny
                if (semaphore_p(semID, 2) == -1) {
                    perror("Błąd semaphore_p w kasjer");
                    pthread_exit(NULL);
                }
                
                int flaga = aktywne_kasy[index];
                
                if (semaphore_v(semID, 2) == -1) {
                    perror("Błąd semaphore_p w kasjer");
                    pthread_exit(NULL);
                }

                if (flaga == 0) {
                    // Sprawdzenie, czy kolejka komunikatów jest pusta
                    struct msqid_ds buf;
                    if (msgctl(kasjer_id, IPC_STAT, &buf) == -1) {
                        perror("Błąd pobierania stanu kolejki");
                        pthread_exit(NULL);
                    }

                    if (buf.msg_qnum == 0) {
                        // Kolejka jest pusta, kasjer może zakończyć działanie
                        printf("\tKasjer %d o ID: %d kończy pracę ponieważ kolejka jest pusta.\n", index, kasjer_id);
                        pthread_exit(NULL);
                    }
                }

                sleep(3);
                continue;
            } else if (errno == EINTR) {
                printf("EINTR");
                continue;
            }
        }
        int czas = rand() % 8 + 3;
        printf("\tKasjer %d obsługuje klienta %d przez %d sekund\n", index + 1, msg.klient_id, czas);
        sleep(czas);
        msg.mtype = msg.klient_id;
        if (msgsnd(kasjer_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("Błąd wysyłania komunikatu od kasjera do klienta");
            break;
        }
    }
    pthread_exit(NULL);
}

// Funkcja monitorująca stan kas
void aktualizuj_kasy() {
    if (semaphore_p(semID, 0) == -1) {
        perror("Błąd semaphore_p w aktualizuj_kasy");
        return;
    }

    //printf("sprawdzanie liczby kas....\n");
    if (sklep->liczba_klientow / K >= sklep->liczba_kas && sklep->liczba_kas < MAX_KASY) {
        sklep->liczba_kas++;
        if (!aktywne_kasy[sklep->liczba_kas - 1]) { //dla flag=1
            pthread_create(&kasjerzy[sklep->liczba_kas - 1], NULL, kasjer, (void *)(long)(sklep->liczba_kas - 1));
            aktywne_kasy[sklep->liczba_kas - 1] = 1;
        }
    } else if (sklep->liczba_klientow < K * (sklep->liczba_kas - 1) && sklep->liczba_kas > 2) {
        sklep->liczba_kas--;
        if (aktywne_kasy[sklep->liczba_kas]) {
            //pthread_join(kasjerzy[sklep->liczba_kas], NULL);
            aktywne_kasy[sklep->liczba_kas] = 0;
        }
    }
    printf("Aktualna liczba kas: %d\n", sklep->liczba_kas);

    if (semaphore_v(semID, 0) == -1) { // Semafor 0 (dostęp do pamięci dzielonej)
        perror("Błąd semaphore_v w aktualizuj_kasy");
        return;
    }

    //printf("koniec sprawdzania liczby kas....\n");
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
    /*
    // Rejestracja obsługi sygnału SIGINT
    if (signal(SIGINT, cashier_signal_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału");
        if (shmdt(sklep) == -1) {
            perror("Błąd odłączania segmentu pamięci dzielonej");
            exit(1);
        }
        exit(1);
    }
    */
    // Tworzenie 2 początkowych kas
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&kasjerzy[i], NULL, kasjer, (void *)(long)i) != 0) {
            perror("Błąd tworzenia wątku kasjera");
            if (shmdt(sklep) == -1) {
                perror("Błąd odłączania segmentu pamięci dzielonej");
                exit(1);
            }
            exit(1);
        }
        aktywne_kasy[i] = 1;
    }
    sleep(1);
    // Pętla monitorująca stan kas
    while (!check_fire_flag(sklep)) {
        aktualizuj_kasy();
        sleep(2); 
    }

    // Zakończenie programu ()
    for (int i = 0; i < MAX_KASY; i++) {
        if (aktywne_kasy[i] != 0 )  {
            if (pthread_join(aktywne_kasy[i], NULL) != 0) {
                perror("Błąd oczekiwania na zakończenie wątku kasjera");
            } else {
                printf("zakończono wątek kasjera: %d", aktywne_kasy[i]);
            }
        }
    }

    // Odłączenie pamięci współdzielonej
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }

    return 0;
}
