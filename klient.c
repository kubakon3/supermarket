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
extern int aktywne_kasy[MAX_KASY];

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
    
    // Semafor dla funkcji aktualizującej kasy
    if (semaphore_v(semID, 4) == -1) {
        perror("Błąd semaphore_v(4) w increment_customers");
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
    
    // Semafor dla funkcji aktualizującej kasy
    if (semaphore_v(semID, 4) == -1) {
        perror("Błąd semaphore_v(4) w decrement_customers");
    }
}

void customer_signal_handler() {
    printf("Klient otrzymał sygnał SIGUSR2. Kończenie pracy...\n");
    
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }
    exit(0);
}

int main() {
    srand(time(NULL));
    signal(SIGINT, SIG_IGN);
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
    
    // Rejestracja obsługi sygnału SIGUSR2
    if (signal(SIGUSR2, customer_signal_handler) == SIG_ERR) {
        perror("Błąd rejestracji obsługi sygnału");
        if (shmdt(sklep) == -1) {
            perror("Błąd odłączania segmentu pamięci dzielonej");
            exit(1);
        }
        exit(1);
    }

    // Symulacja klienta
    increment_customers();
    pid = getpid();
    int czas = rand() % 3;
    printf("Klient %d wchodzi do sklepu i robi zakupy przez %d sekund\n", pid, czas);
    // sleep(czas);

    // Wybór kasy z najkrótszą kolejką
    int cashier_msg_id = -1; // index id kolejki komunikatów
    int cashier_id = 0; // numer kasjera 1 do 10
    int min_message_count = INT_MAX;

    for (int i = 0; i < sklep->liczba_kas; i++) {
        struct msqid_ds buf;
        int id = get_queue_id(i);
        if (msgctl(id, IPC_STAT, &buf) == -1) {
            perror("Błąd pobierania statystyk kolejki komunikatów");
            shmdt(sklep);
            if (semaphore_v(semID, 1) == -1) {
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

    printf("Klient %d ustawił się w kolejce %d o ID:  %d\n", pid, cashier_id, sklep->kolejki_kas[cashier_msg_id]);
    if (msgsnd(msg.mtype, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu klienta");
        shmdt(sklep);
        if (semaphore_v(semID, 1) == -1) {
            perror("Błąd zwiększania semafora klientów");
            exit(1);
        }
        exit(1);
    }

    // Oczekiwanie na odpowiedź od kasjera
    while (1) {
        if (msgrcv(msg.mtype, &msg, sizeof(msg) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                // usleep(10000);
                continue;
            } else {
                perror("Błąd odbierania komunikatu od kasjera");
                shmdt(sklep);
                if (semaphore_v(semID, 1) == -1) {
                    perror("Błąd zwiększania semafora klientów");
                }
                exit(1);
            }
        } else {
            break;
        }
    }

    printf("  Klient %d został obsłużony i wychodzi ze sklepu\n", pid);
    decrement_customers();

    if (semaphore_v(semID, 1) == -1) {
        perror("Błąd zwiększania semafora klientów");
        shmdt(sklep);
        exit(1);
    }
    
    if (shmdt(sklep) == -1) {
        perror("Błąd odłączania segmentu pamięci dzielonej");
        exit(1);
    }

    return 0;
}
