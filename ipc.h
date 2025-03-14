#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

// Definicje stałych
#define MAX_KASY 10
#define K 5
#define MAX_KLIENTOW 50
#define SHM_KEY 2953
#define SEM_KEY 3092

// Struktura dla dzielonej pamięci
typedef struct {
    int liczba_klientow;
    int liczba_kas;
    int kolejki_kas[MAX_KASY]; // id kolejek komunikatów
    int fire_flag;
    pid_t klienci_pidy[MAX_KLIENTOW]; // Tablica PID-ów klientów
} Sklep;

// Struktura komunikatu
typedef struct {
    long mtype;
    pid_t klient_id;
} Komunikat;

// Funkcje do zarządzania pamięcią dzieloną
Sklep *get_shared_memory();
void destroy_shared_memory(Sklep *sklep);

// Funkcje do zarządzania danymi w pamięci dzielonej
int get_queue_id(int index);
int get_cashiers();
int get_active_cashier(int index);
void set_active_cashier(int index);
void set_inactive_cashier(int index);

// Funkcje do zarządzania semaforami
int create_semaphore();
void set_semaphore(int semID, int number, int value);
int semaphore_p(int semID, int number);
int semaphore_v(int semID, int number);
void remove_semaphore(int semID);

int check_fire_flag(Sklep *sklep);

#endif // IPC_H
