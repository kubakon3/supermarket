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
extern int semID;
pid_t pid_kierownik;
pthread_t tid_strazak;
pthread_t tid_zombie_cleanup;
pid_t klienci_pidy[MAX_KLIENTOW] = {0};


// Funkcja czekająca na Ctrl+C
void signal_handler(int sig) {
    if (sig == SIGINT) {
        if (pthread_kill(tid_strazak, SIGUSR1) != 0) {
            perror("Błąd wysłania SIGUSR1 do strażaka");
            exit(1);
        }
    }
}

// Wątek do zbierania procesów zombie klientów
void *zombie_cleanup_thread(void *arg) {
    (void)arg;
    printf("Wątek do zbierania zombie uruchomiony\n");
    pid_t pidy_kopia[MAX_KLIENTOW];
    int fire_detected = 0;
    int timeout_counter = 0;
    
    while (1) {
        if (!fire_detected && check_fire_flag(sklep)) {
            printf("Zombie cleanup: Wykryto pożar, czekam na zakończenie wszystkich klientów\n");
            fire_detected = 1;
            timeout_counter = 0;
        }
        
        if (semaphore_p(semID, 5) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Błąd semaphore_p w zombie_cleanup_thread");
            break;
        }
        
        int wszyscy_zakonczyli = 1;
        int pozostali_klienci = 0;
        for (int i = 0; i < MAX_KLIENTOW; i++) {
            pidy_kopia[i] = klienci_pidy[i];
            if (klienci_pidy[i] > 0) {
                wszyscy_zakonczyli = 0;
                pozostali_klienci++;
            }
        }
        
        if (semaphore_v(semID, 5) == -1) {
            perror("Błąd semaphore_v w zombie_cleanup_thread");
            break;
        }

        if (fire_detected && wszyscy_zakonczyli) {
            printf("Zombie cleanup: Wszyscy klienci zakończyli pracę\n");
            break;
        }
        
        if (fire_detected && timeout_counter++ > 1000) {
            printf("Zombie cleanup: TIMEOUT! Pozostało %d procesów klientów:\n", pozostali_klienci);
            for (int i = 0; i < MAX_KLIENTOW; i++) {
                if (pidy_kopia[i] > 0) {
                    printf("  - PID %d (slot %d)\n", pidy_kopia[i], i);
                }
            }
            printf("Wymuszam zakończenie wątku zombie_cleanup\n");
            break;
        }

        for (int i = 0; i < MAX_KLIENTOW; i++) {
            if (pidy_kopia[i] > 0) {
                int status;
                pid_t result = waitpid(pidy_kopia[i], &status, WNOHANG);
                
                if (result > 0) {
                    printf("Zombie cleanup: zebrany proces PID %d ze statusem %d\n", result, WEXITSTATUS(status));
                    
                    if (semaphore_p(semID, 5) == -1) { 
                        perror("Błąd semaphore_p przy usuwaniu PID");
                        break;
                    }
                    
                    klienci_pidy[i] = 0;
                    
                    if (semaphore_v(semID, 5) == -1) {
                        perror("Błąd semaphore_v przy usuwaniu PID");
                    }
                } else if (result == -1) {
                    if (errno == ECHILD) {
                        printf("Zombie cleanup: Proces PID %d już nie istnieje (ECHILD)\n", pidy_kopia[i]);
                        if (semaphore_p(semID, 5) == -1) { 
                            perror("Błąd semaphore_p przy usuwaniu PID");
                            break;
                        }
                        klienci_pidy[i] = 0;
                        if (semaphore_v(semID, 5) == -1) {
                            perror("Błąd semaphore_v przy usuwaniu PID");
                        }
                    } else {
                        perror("Błąd waitpid w zombie_cleanup_thread");
                    }
                } else if (result == 0 && fire_detected && timeout_counter > 500) {
                    printf("Zombie cleanup: Wymuszam zabicie procesu PID %d (SIGKILL)\n", pidy_kopia[i]);
                    kill(pidy_kopia[i], SIGKILL);
                }
            }
        }

        // usleep(10000);
    }
    pthread_exit(NULL);
}



int main() {
    pid_t pid = getpid();
    setpgid(pid,pid);
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
    set_semaphore(semID, 2, 1); // Semafor 2 (dostęp do aktywne_kasy)
    set_semaphore(semID, 3, 1); // Semafor 3 (synchronizacja tworzenia procesów)
    set_semaphore(semID, 4, 0); // Semafor 4 (event counter dla kierownika)
    set_semaphore(semID, 5, 1); // Semafor 5 (dostęp do tablicy klienci_pidy)

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
        printf("Utworzono kolejkę komunikatów %d: ID = %d\n", i+1, sklep->kolejki_kas[i]);
    }
    
    // Rejestracja obsługi sygnału SIGINT
    struct sigaction sa_sigint;
    sa_sigint.sa_handler = signal_handler;
    sigemptyset(&sa_sigint.sa_mask);
    sa_sigint.sa_flags = 0;
    if (sigaction(SIGINT, &sa_sigint, NULL) == -1) {
        perror("Błąd rejestracji obsługi sygnału SIGINT");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    // Synchronizacja przed tworzeniem procesów
    if (semaphore_p(semID, 3) == -1) {
        perror("Błąd semaphore_p przed tworzeniem strażaka");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    // Tworzenie wątku strażaka
    if (pthread_create(&tid_strazak, NULL, (void*)strazak, NULL) != 0) {
        perror("Błąd tworzenia wątku strażaka");
        semaphore_v(semID, 3);
        kill(pid_kierownik, SIGTERM);
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    if (semaphore_v(semID, 3) == -1) {
        perror("Błąd semaphore_v po utworzeniu strażaka");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    // Synchronizacja przed tworzeniem kierownika
    if (semaphore_p(semID, 3) == -1) {
        perror("Błąd semaphore_p przed tworzeniem kierownika");
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
    
    if (semaphore_v(semID, 3) == -1) {
        perror("Błąd semaphore_v po utworzeniu kierownika");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }

    if (semaphore_p(semID, 3) == -1) {
        perror("Błąd semaphore_p przed tworzeniem kierownika");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    // Tworzenie wątku do zbierania zombie procesów
    if (pthread_create(&tid_zombie_cleanup, NULL, zombie_cleanup_thread, NULL) != 0) {
        perror("Błąd tworzenia wątku zombie_cleanup");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }

    if (semaphore_v(semID, 3) == -1) {
        perror("Błąd semaphore_v po utworzeniu kierownika");
        for (int i = 0; i < MAX_KASY; i++) {
            msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);
        }
        remove_semaphore(semID);
        destroy_shared_memory(sklep);
        exit(1);
    }
    
    // sleep(1);
    //for(int i = 0; i < 20; i++)
    //while (!check_fire_flag(sklep))
    // Tworzenie procesów klientów 
    while (!check_fire_flag(sklep)) {
        
        // Semafor pilnujący ilości miejsc w sklepie
        if (semaphore_p(semID, 1) == -1) {
            printf("Sklep jest pełen");
            // sleep(4);
            continue;
        }

        pid_t pid_klient = fork();
        if (pid_klient == -1) {
            perror("Błąd forkowania procesu klienta");
            semaphore_v(semID, 1);
            _exit(1);
            // sleep(4);
            continue;
        } else if (pid_klient == 0) {
            if (execl("./klient", "klient", NULL) == -1) {
                perror("Błąd uruchamiania klient");
                _exit(1);
            }
        } else {
            if (semaphore_p(semID, 5) == -1) { 
                perror("Błąd semaphore_p w main");
                exit(1);
            }
            for (int i = 0; i < MAX_KLIENTOW; i++) {
                if (klienci_pidy[i] == 0) {
                    klienci_pidy[i] = pid_klient;
                    break;
                }
            }
            if (semaphore_v(semID, 5) == -1) { 
                perror("Błąd semaphore_v w main");
                exit(1);
            }
        }
        // sleep(1);
    }
    if(getpid() != pid) {
        return 0;
    }
    
    // sleep(3);

    // Czekanie na zakończenie wątku zombie_cleanup
    if (pthread_join(tid_zombie_cleanup, NULL) != 0) {
        perror("Błąd oczekiwania na zakończenie wątku zombie_cleanup");
    } else {
        printf("Zakończono wątek zombie_cleanup\n");
    }
    // sleep(1);
    
    // Czekanie na zakończenie kierownika
    if (waitpid(pid_kierownik, NULL, 0) == -1) {
        perror("Błąd oczekiwania na zakończenie procesu kierownika kasjerów");
    } else {
        printf("zakończono proces kierownika kasjerów\n");
    }
    
    // Czekanie na zakończenie wątku strażaka
    if (pthread_join(tid_strazak, NULL) != 0) {
        perror("Błąd oczekiwania na zakończenie wątku strażaka");
    } else {
        printf("zakończono wątek strażaka\n");
    }

    // Czyszczenie mechanizmów komunikacji
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
