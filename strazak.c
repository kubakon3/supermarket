#include "strazak.h"

// Funkcja do obsługi sygnału SIGUSR1
void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Strażak: Otrzymano sygnał SIGUSR1. Wysyłanie sygnału SIGUSR2 do wszystkich procesów potomnych...\n");

        // Wysyłanie sygnału SIGUSR2 do wszystkich procesów potomnych w grupie
        if (kill(0, SIGUSR2) == -1) {
            perror("Błąd wysyłania sygnału SIGUSR2 do procesów potomnych");
        }

        printf("Strażak: Sygnał SIGUSR2 wysłany. Kończenie pracy wątku strażaka.\n");
        pthread_exit(NULL);
    }
}

// Funkcja wykonywana przez wątek strażaka
void *strazak(void *arg) {
    if (signal(SIGUSR1, sigusr_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału SIGUSR1");
        pthread_exit(NULL);
    }

    printf("Strażak: Wątek strażaka uruchomiony. Oczekiwanie na sygnał SIGINT (Ctrl+C)...\n");

    while (1) {
        pause();
    }

    pthread_exit(NULL);
}
