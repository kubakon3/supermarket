#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "ipc.h"
#include "strazak.h"

extern Sklep *sklep;
extern int semID; // ID semafora

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("Pożar! Wszyscy klienci opuszczają sklep. Zamykamy kasy!\n");
        sklep->liczba_klientow = 0;
        sklep->liczba_kas = 2;
    }
}

int main() {
    // Inicjalizacja pamięci dzielonej
    sklep = get_shared_memory();
    if (sklep == NULL) {
        exit(1);
    }

    sklep->liczba_klientow = 0;
    sklep->liczba_kas = 2;

    // Inicjalizacja semaforów
    semID = create_semaphore();
    if (semID == -1) {
        destroy_shared_memory(sklep);
        exit(1);
    }

    // Ustawienie wartości semaforów
    set_semaphore(semID, 0, 1); // Semafor 0 (dostęp do pamięci dzielonej)
    set_semaphore(semID, 1, MAX_KLIENTOW); // Semafor 1 (liczba klientów)

    // Inicjalizacja kolejek komunikatów dla kas
    for (int i = 0; i < MAX_KASY; i++) {
        sklep->kolejki_kas[i] = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        if (sklep->kolejki_kas[i] == -1) {
            perror("Błąd tworzenia kolejki komunikatów");
            for (int j = 0; j < i; j++) {
                msgctl(sklep->kolejki_kas[j], IPC_RMID, NULL);
            }
            remove_semaphore(semID);
            destroy_shared_memory(sklep);
            exit(1);
        }
    }

    // Rejestracja obsługi sygnału SIGINT
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }

    // Tworzenie procesu kierownika kasjerów
    pid_t pid_kierownik = fork();
    if (pid_kierownik == -1) {
        perror("Błąd forkowania procesu kierownika kasjerów");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    } else if (pid_kierownik == 0) {
        if (execl("./kierownik_kasjerow", "kierownik_kasjerow", NULL) == -1) {
            perror("Błąd uruchamiania kierownik_kasjerow");
            exit(1);
        }
    }

    // Tworzenie wątku strażaka
    pthread_t tid_strazak;
    if (pthread_create(&tid_strazak, NULL, (void*)strazak, NULL) != 0) {
        perror("Błąd tworzenia wątku strażaka");
        kill(pid_kierownik, SIGTERM);
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }

    // Tworzenie procesów klientów 
    while (1) {
        // Sprawdzanie ilości miejsc w sklepie
        if (semaphore_p(semID, 1) == -1) { // Semafor 1 (liczba klientów)
            perror("Sklep jest pełen");
            sleep(4);
            continue;
        }

        pid_t pid_klient = fork();
        if (pid_klient == -1) {
            perror("Błąd forkowania procesu klienta");
            semaphore_v(semID, 1); // Zwolnienie miejsca w sklepie
            sleep(4);
            continue;
        } else if (pid_klient == 0) {
            if (execl("./klient", "klient", NULL) == -1) {
                perror("Błąd uruchamiania klient");
                exit(1);
            }
        }
        
        sleep(rand() % 3 + 1);
    }

    // Zakończenie wątku strażaka ()
    if (pthread_cancel(tid_strazak) != 0) {
        perror("Błąd zakończenia wątku strażaka");
    }
    if (pthread_join(tid_strazak, NULL) != 0) {
        perror("Błąd oczekiwania na zakończenie wątku strażaka");
    }

    // Zakończenie procesu kierownika kasjerów ()
    if (kill(pid_kierownik, SIGTERM) == -1) {
        perror("Błąd zakończenia procesu kierownika kasjerów");
    }
    if (waitpid(pid_kierownik, NULL, 0) == -1) {
        perror("Błąd oczekiwania na zakończenie procesu kierownika kasjerów");
    }

    // Czyszczenie mechanizmów komunikacji ()
    for (int i = 0; i < MAX_KASY; i++) {
        if (msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania kolejki komunikatów");
        }
    }

    // Usunięcie semaforów
    remove_semaphore(semID);

    // Usunięcie pamięci dzielonej
    destroy_shared_memory(sklep);

    printf("Program zakończony.\n");
    return 0;
}
