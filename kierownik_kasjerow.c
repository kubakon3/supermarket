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

// Funkcja obsługi sygnału SIGINT w kierowniku kasjerów
void kierownik_signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("Kierownik kasjerów otrzymał sygnał SIGINT. Kończenie pracy...\n");

        for (int i = 0; i < MAX_KASY; i++) {
            int flag = get_active_cashier(i);
            if (flag == 1) {
                void *status;
                if (pthread_join(kasjerzy[i], &status) == -1) {
                    perror("Błąd oczekiwania na zakończenie wątku kasjera");
                } else {
                    printf("Kasjer %d zakończył działanie z statusem: %ld\n", i + 1, (long)status);
                }
                
            }
        }
        exit(0);
    }
}

// Funkcja wykonywana przez wątek kasjera
void *kasjer(void *arg) {
    srand(time(NULL));
    int index = (int)(long)arg;
    int kasjer_id = get_queue_id(index);
    printf("\t\tAktywny kasjer: %d o id: %d \n", index + 1, kasjer_id);
    Komunikat msg;
    while (!check_fire_flag(sklep)) {
        if (msgrcv(kasjer_id, &msg, sizeof(msg) - sizeof(long), kasjer_id, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {

                int flaga = get_active_cashier(index);
                
                if (flaga == 0) {
                    struct msqid_ds buf;
                    if (msgctl(kasjer_id, IPC_STAT, &buf) == -1) {
                        perror("Błąd pobierania stanu kolejki");
                        pthread_exit(0);
                    }

                    if (buf.msg_qnum == 0) {
                        printf("\tKasjer %d o ID: %d kończy pracę ponieważ kolejka jest pusta.\n", index + 1, kasjer_id);
                        pthread_exit(0);
                    }
                }
                usleep(10000);
                continue;
            } else if (errno == EINTR) {
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
    pthread_exit(0);
}

// Funkcja monitorująca stan kas
void aktualizuj_kasy() {
    if (semaphore_p(semID, 0) == -1) {
        perror("Błąd semaphore_p w aktualizuj_kasy");
        return;
    }
    
    int active_cashier;

    if (sklep->liczba_klientow / K >= sklep->liczba_kas && sklep->liczba_kas < MAX_KASY) {
        sklep->liczba_kas++;
        active_cashier = get_active_cashier(sklep->liczba_kas - 1); // pobranie flagi kasjera
        if (!active_cashier) { //dla flag = 0
            pthread_create(&kasjerzy[sklep->liczba_kas - 1], NULL, kasjer, (void *)(long)(sklep->liczba_kas - 1));
            set_active_cashier(sklep->liczba_kas - 1); // ustawienie flagi
        }
    } else if (sklep->liczba_klientow < K * (sklep->liczba_kas - 1) && sklep->liczba_kas > 2) {
        sklep->liczba_kas--;
        active_cashier = get_active_cashier(sklep->liczba_kas); // pobranie flagi kasjera
        if (active_cashier) {
            set_inactive_cashier(sklep->liczba_kas);
            printf("\tWysyłanie sygnału zamknięcia do kasy %d\n", sklep->liczba_kas + 1);
        }
    }
    printf("Aktualna liczba kas: %d\n", sklep->liczba_kas);

    if (semaphore_v(semID, 0) == -1) {
        perror("Błąd semaphore_v w aktualizuj_kasy");
        return;
    }
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
    
    // Rejestracja obsługi sygnału SIGINT
    if (signal(SIGINT, kierownik_signal_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału SIGINT");
        exit(1);
    }
    
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
        if (semaphore_p(semID, 4) == -1) {
            perror("Błąd semaphore_p(4) w kierowniku");
            break;
        }
        
        aktualizuj_kasy();
        sleep(4); 
    }

    // Zakończenie programu ()
    for (int i = 0; i < MAX_KASY; i++) {
        if (aktywne_kasy[i] == 1)  {
            if (pthread_join(kasjerzy[i], NULL) != 0) {
                perror("Błąd oczekiwania na zakończenie wątku kasjera");
            } else {
                printf("zakończono wątek kasjera: %d\n", aktywne_kasy[i]);
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
