#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include "ipc.h"
#include "strazak.h"

extern Sklep *sklep;
extern int semID;

// Funkcja do obsługi sygnału SIGUSR1
void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Strażak: Otrzymano sygnał SIGUSR1. Ustawianie flagi pożaru...\n");
        
        // Strażak działa w wątku, więc może bezpiecznie używać semaforów
        if (semaphore_p(semID, 0) == -1) { 
            perror("Błąd semaphore_p w sygnale w main");
            return;
        }
    
        sklep->fire_flag = 1;
    
        if (semaphore_v(semID, 0) == -1) { 
            perror("Błąd semaphore_v w sygnale w main");
            return;
        }
        
        printf("Strażak: Flaga pożaru ustawiona. Kończenie pracy wątku strażaka.\n");
        pthread_exit(NULL);
    }
}

// Funkcja wykonywana przez wątek strażaka
void *strazak(void *arg) {
    struct sigaction sa;
    sa.sa_handler = sigusr_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Błąd rejestracji obsługi sygnału SIGUSR1");
        pthread_exit(NULL);
    }

    printf("Strażak: Wątek strażaka uruchomiony. Oczekiwanie na sygnał SIGINT (Ctrl+C)...\n");

    while (1) {
        pause();
    }

    pthread_exit(NULL);
}
