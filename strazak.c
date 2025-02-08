#include "strazak.h"

// Funkcja do obsługi sygnału SIGINT
void sigint_handler(int sig) {
    if (sig == SIGINT) {
        printf("Strażak: Otrzymano sygnał SIGINT. Wysyłanie sygnału SIGINT do wszystkich procesów potomnych...\n");

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
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału SIGINT");
        pthread_exit(NULL);
    }

    printf("Strażak: Wątek strażaka uruchomiony. Oczekiwanie na sygnał SIGINT (Ctrl+C)...\n");

    // Nieskończona pętla oczekująca na sygnał
    while (1) {
        pause(); // Oczekiwanie na sygnał
    }

    pthread_exit(NULL);
}
