#include "strazak.h"

// Funkcja do obsługi sygnału SIGINT
void sigusr_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Strażak: Otrzymano sygnał SIGUSR1. Wysyłanie sygnału SIGINT do wszystkich procesów potomnych...\n");

        // Wysyłanie sygnału SIGINT do wszystkich procesów potomnych w grupie
        if (kill(0, SIGINT) == -1) {
            perror("Błąd wysyłania sygnału SIGINT do procesów potomnych");
        }

        printf("Strażak: Sygnał SIGINT wysłany. Kończenie pracy wątku strażaka.\n");
        pthread_exit(NULL); // Zakończenie wątku strażaka
    }
}

// Funkcja wykonywana przez wątek strażaka
void *strazak(void *arg) {
    // Rejestracja obsługi sygnału SIGINT
    if (signal(SIGUSR1, sigusr_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału SIGUSR1");
        pthread_exit(NULL);
    }

    printf("Strażak: Wątek strażaka uruchomiony. Oczekiwanie na sygnał SIGINT (Ctrl+C)...\n");

    // Nieskończona pętla oczekująca na sygnał
    while (1) {
        pause(); // Oczekiwanie na sygnał
    }

    pthread_exit(NULL);
}
