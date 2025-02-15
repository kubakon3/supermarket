#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <semaphore.h>
#include "ipc.h"

extern Sklep *sklep;
extern int semID; // ID semafora
int pid;

// Funkcja do zwiększania liczby klientów
void increment_customers() {
    if (semaphore_p(semID, 0) == -1) { 
        perror("Błąd semaphore_p w increment_customers");
        exit(1);
    }
    sklep->liczba_klientow++;
    if (semaphore_v(semID, 0) == -1) { 
        perror("Błąd semaphore_v w increment_customers");
        exit(1);
    }
}

// Funkcja do zmniejszania liczby klientów
void decrement_customers() {
    if (semaphore_p(semID, 0) == -1) { 
        perror("Błąd semaphore_p w decrement_customers");
        exit(1);
    }
    sklep->liczba_klientow--;
    if (semaphore_v(semID, 0) == -1) { 
        perror("Błąd semaphore_v w decrement_customers");
        exit(1);
    }
}

// Funkcja do usuwania PID-u klienta z tablicy w pamięci dzielonej
void remove_pid() {
    if (semaphore_p(semID, 0) == -1) { // Semafor 0 (dostęp do pamięci dzielonej)
        perror("Błąd semaphore_p w remove_pid");
        shmdt(sklep);
        exit(1);
    }
    for (int i = 0; i < MAX_KLIENTOW; i++) {
        if (sklep->klienci_pidy[i] == pid) {
            sklep->klienci_pidy[i] = 0;
            break;
        }
    }
    if (semaphore_v(semID, 0) == -1) { // Semafor 0 (dostęp do pamięci dzielonej)
        perror("Błąd semaphore_v w remove_pid");
        shmdt(sklep);
        exit(1);
    }
}

void customer_signal_handler(int sig) {
    
    decrement_customers();
    
    //ilość klientów w sklepie
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
    exit(0);
}

int main() {
    srand(time(NULL));

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
    if (signal(SIGINT, customer_signal_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału");
        if (shmdt(sklep) == -1) {
            perror("Błąd odłączania segmentu pamięci dzielonej");
            exit(1);
        }
        exit(1);
    }

    // Symulacja klienta
    int czas = rand() % 3 + 1;
    increment_customers(); // Zwiększenie liczby klientów
    pid = getpid();

    // Wybór kasy z najkrótszą kolejką
    int cashier_msg_id = -1; // indeks kasjera
    int cashier_id = 0; // numer kasjera 1 do 10
    int min_message_count = INT_MAX;

    for (int i = 0; i < sklep->liczba_kas; i++) {
        struct msqid_ds buf;
        if (msgctl(sklep->kolejki_kas[i], IPC_STAT, &buf) == -1) {
            perror("Błąd pobierania statystyk kolejki komunikatów");
            shmdt(sklep);
            if (semaphore_v(semID, 1) == -1) { // Semafor 1 (liczba klientów)
                perror("Błąd zwiększania semafora klientów");
                exit(1);
            }
            exit(1);
        }
        int messages = buf.msg_qnum;
        if (messages < min_message_count) {
            min_message_count = messages;
            cashier_msg_id = i;
            cashier_id = i + 1;
        }
    }

    // Wysłanie komunikatu do wybranej kasy
    Komunikat msg;
    msg.mtype = sklep->kolejki_kas[cashier_msg_id];
    msg.klient_id = pid;

    if (msgsnd(sklep->kolejki_kas[cashier_msg_id], &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu klienta");
        shmdt(sklep);
        if (semaphore_v(semID, 1) == -1) { // Semafor 1 (liczba klientów)
            perror("Błąd zwiększania semafora klientów");
            exit(1);
        }
        exit(1);
    }

    printf("Klient %d ustawił się w kolejce %d do kasjera %d\n", pid, cashier_id, sklep->kolejki_kas[cashier_msg_id]);

    // Oczekiwanie na odpowiedź od kasjera
    while (1) {
        if (msgrcv(sklep->kolejki_kas[cashier_msg_id], &msg, sizeof(msg) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(3);
                continue;
            } else {
                perror("Błąd odbierania komunikatu od kasjera");
                shmdt(sklep);
                if (semaphore_v(semID, 1) == -1) { // Semafor 1 (liczba klientów)
                    perror("Błąd zwiększania semafora klientów");
                }
                exit(1);
            }
        } else {
            break;
        }
    }

    printf("Klient %d został obsłużony i wychodzi ze sklepu\n", pid);
    decrement_customers(); // Zmniejszenie liczby klientów
    printf("Liczba klientów: %d\n", sklep->liczba_klientow);

    // Zwiększenie wartości semafora o 1 (zwolnienie miejsca dla nowego klienta)
    if (semaphore_v(semID, 1) == -1) { // Semafor 1 (liczba klientów)
        perror("Błąd zwiększania semafora klientów");
        shmdt(sklep);
        exit(1);
    }
    
    remove_pid();

    // Odłączenie pamięci dzielonej
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }

    return 0;
}
