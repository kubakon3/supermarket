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
pid_t pid_kierownik;
pthread_t tid_strazak;

void signal_handler(int sig) {
    if (semaphore_p(semID, 0) == -1) { // dostęp do pamięci dzielonej
        perror("Błąd semaphore_p w sygnale");
        return;
    }
    
    sklep->fire_flag = 1;
    
    if (semaphore_v(semID, 0) == -1) { // dostęp do pamięci dzielonej
        perror("Błąd semaphore_v w sygnale");
        return;
    }
    
    if (pthread_kill(tid_strazak, SIGUSR1) != 0) {
        perror("Błąd wysłania SIGUSR1 do strażaka");
        exit(1);
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
    sklep->fire_flag = 0;

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
    pid_kierownik = fork();
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
    while (!check_fire_flag(sklep)) {
        // Sprawdzanie ilości miejsc w sklepie
        if (semaphore_p(semID, 1) == -1) { // Semafor 1 (liczba klientów)
            printf("Sklep jest pełen");
            sleep(4);
            continue;
        }

        pid_t pid_klient = fork();
        if (pid_klient == -1) {
            perror("Błąd forkowania procesu klienta");
            semaphore_v(semID, 1);
            sleep(4);
            continue;
        } else if (pid_klient == 0) {
            if (execl("./klient", "klient", NULL) == -1) {
                perror("Błąd uruchamiania klient");
                exit(1);
            }
        } else {
            if (semaphore_p(semID, 0) == -1) { 
                perror("Błąd semaphore_p w main");
                exit(1);
            }
            for (int i = 0; i < MAX_KLIENTOW; i++) {
                if (sklep->klienci_pidy[i] == 0) {
                    sklep->klienci_pidy[i] = pid_klient;
                    break;
                }
            }
            if (semaphore_v(semID, 0) == -1) { 
                perror("Błąd semaphore_v w main");
                exit(1);
            }
        }
        sleep(1);
    }

    // Zakończenie wątku strażaka ()
    if (pthread_join(tid_strazak, NULL) != 0) {
        perror("Błąd oczekiwania na zakończenie wątku strażaka");
    } else {
        printf("zakończono wątek strażaka\n");
    }

    if (waitpid(pid_kierownik, NULL, 0) == -1) {
        perror("Błąd oczekiwania na zakończenie procesu kierownika kasjerów");
    } else {
        printf("zakończono proces kierownika kasjerów\n");
    }

    // Czyszczenie mechanizmów komunikacji ()
    for (int i = 0; i < MAX_KASY; i++) {
        if (msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL) == -1) {
            perror("Błąd usuwania kolejki komunikatów");
        }  else {
        printf("usunięto kasę\n");
        }
    }

    // Usunięcie semaforów
    remove_semaphore(semID);

    // Usunięcie pamięci dzielonej
    destroy_shared_memory(sklep);

    printf("Program zakończony.\n");
    return 0;
}
