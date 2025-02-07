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
#include <limits.h>
#include <errno.h>

#define MAX_KASY 10
#define K 5
#define MAX_KLIENTOW 50

// Struktura dla współdzielonej pamięci
typedef struct {
    int liczba_klientow;
    int liczba_kas;
    int kolejki_kas[MAX_KASY]; // id kolejek komunikatów
} Sklep;

// Struktura komunikatu
typedef struct {
    long mtype;
    pthread_t klient_id;
} Komunikat;

int shm_id;
Sklep *sklep;
sem_t semafor;
pthread_t kasjerzy[MAX_KASY];
int aktywne_kasy[MAX_KASY] = {0};

void *kasjer(void *arg) {
    srand(time(NULL));
    int kasjer_id = (int)(long)arg;
    pthread_t pid = pthread_self();
    printf("\t\tAktywny kasjer: %d o pid: %lu \n",kasjer_id, pid);
    Komunikat msg;
    while (1) {
        if (msgrcv(kasjer_id, &msg, sizeof(msg) - sizeof(long), kasjer_id, IPC_NOWAIT) != -1) {
            int czas = rand() % 8 + 3;
            printf("Kasjer %d obsługuje klienta %lu przez %d sekund\n", kasjer_id, msg.klient_id, czas);
            sleep(czas);
            msg.mtype = msg.klient_id;
            msgsnd(kasjer_id, &msg, sizeof(msg) - sizeof(long), 0);
        } else if (errno == ENOMSG) {
            sleep(3);
            continue;
        } else {
            perror("Błąd odbierania komunikatu klient od kasjera");
            break;
        }   
    }
    pthread_exit(NULL);
}

void aktualizuj_kasy() {
    sem_wait(&semafor);
    if (sklep->liczba_klientow / K >= sklep->liczba_kas && sklep->liczba_kas < MAX_KASY) {
        sklep->liczba_kas++;
        if (!aktywne_kasy[sklep->liczba_kas - 1]) {
            pthread_create(&kasjerzy[sklep->liczba_kas - 1], NULL, kasjer, (void *)(long)(sklep->liczba_kas - 1));
            aktywne_kasy[sklep->liczba_kas - 1] = 1;
        }
    } else if (sklep->liczba_klientow < K * (sklep->liczba_kas - 1) && sklep->liczba_kas > 2) {
        sklep->liczba_kas--;
        if (aktywne_kasy[sklep->liczba_kas]) {
            pthread_cancel(kasjerzy[sklep->liczba_kas]);
            aktywne_kasy[sklep->liczba_kas] = 0;
        }
    }
    printf("Aktualna liczba kas: %d\n", sklep->liczba_kas);
    sem_post(&semafor);
}

void *klient(void *arg) {
    srand(time(NULL));
    int czas = rand() % 3 + 1;
    sklep->liczba_klientow++;
    pthread_t pid = pthread_self();
    //printf("klient %lu wchodzi do sklepu, robi zakupy przez: %d, liczba klientow: %d\n", pid, czas, sklep->liczba_klientow);
    //sleep(czas);
    int cashier_msg_id = -1; // indeks ksjera
    int cashier_id = 0; //numer kasjera 1 do 10
    int min_message_count = INT_MAX; 
    for (int i = 0; i < sklep->liczba_kas; i++) {
        int queue_id = sklep->kolejki_kas[i];
        struct msqid_ds buf;
        msgctl(sklep->kolejki_kas[i], IPC_STAT, &buf);
        int messages = buf.msg_qnum;
        if (messages < min_message_count) {
            min_message_count = messages;
            cashier_msg_id = i;
            cashier_id = i + 1;
        }
    }
    //potrzeba id kolejki
    Komunikat msg;
    msg.mtype = sklep->kolejki_kas[cashier_msg_id];
    msg.klient_id = pid;
    if (msgsnd(sklep->kolejki_kas[cashier_msg_id], &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("Błąd wysyłania komunikatu");
        pthread_exit(NULL);
    }
    printf("Klient %lu ustawił się w kolejce %d do kasjera %d\n", pid, cashier_id, sklep->kolejki_kas[cashier_msg_id]);
    while (1) {
        if (msgrcv(sklep->kolejki_kas[cashier_msg_id], &msg, sizeof(msg) - sizeof(long), pid, IPC_NOWAIT) == -1) {
            if (errno == ENOMSG) {
                sleep(3);
                continue;
            } else {
                perror("Błąd odbierania komunikatu klient od kasjera");
                pthread_exit(NULL);
            }
        } else {
            break;
        }
    }
    printf("Klient %lu został obsłużony i wychodzi ze sklepu\n", pid);
    sklep->liczba_klientow--;
    printf("liczba klientow: %d\n", sklep->liczba_klientow);
    pthread_exit(NULL);
}

void *kierownik(void *arg) {
    while (1) {
        sleep(3);
        aktualizuj_kasy();
    }
    pthread_exit(NULL);
}

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        printf("Pożar! Wszyscy klienci opuszczają sklep. Zamykamy kasy!\n");
        sklep->liczba_klientow = 0;
        sklep->liczba_kas = 2;
    }
}

void strażak() {
    sleep(20);
    kill(getppid(), SIGUSR1);
}

int main() {
    srand(time(NULL));
    key_t klucz = ftok("supermarket", 65);
    shm_id = shmget(klucz, sizeof(Sklep), 0666 | IPC_CREAT);
    sklep = (Sklep *)shmat(shm_id, NULL, 0);
    sklep->liczba_klientow = 0;
    sklep->liczba_kas = 2;
    sem_init(&semafor, 1, 1);
    signal(SIGUSR1, signal_handler);
    
    for (int i = 0; i < MAX_KASY; i++) {
        sklep->kolejki_kas[i] = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    }
    
    //stworzenie wątku kierownika
    pthread_t tid_kierownik;
    pthread_create(&tid_kierownik, NULL, kierownik, NULL);
    
    for (int i = 0; i < 2; i++) {
        pthread_create(&kasjerzy[i], NULL, kasjer, (void *)(long)sklep->kolejki_kas[i]);
        aktywne_kasy[i] = 1;
    }
    
    pthread_t klienci[MAX_KLIENTOW];
    for (long i = 0; i < MAX_KLIENTOW; i++) {
        pthread_create(&klienci[i], NULL, klient, (void *)i);
        sleep(rand() % 3 + 1);
    }
    
    if (fork() == 0) {
        strażak();
        exit(0);
    }
    
    //czyszczenie
    for (int i = 0; i < MAX_KLIENTOW; i++) {
        pthread_join(klienci[i], NULL);//czekanie jak się skończą
    }
    
    pthread_cancel(tid_kierownik);//usuwanie wątku kierownika
    for (int i = 0; i < MAX_KASY; i++) {
        if (aktywne_kasy[i]) {
            pthread_cancel(kasjerzy[i]);//usuwanie wątku aktywej kasy
        }
        msgctl(sklep->kolejki_kas[i], IPC_RMID, NULL);//usuwanie kolejek wszystkich kas
    }
    sem_destroy(&semafor);
    shmdt(sklep);
    shmctl(shm_id, IPC_RMID, NULL);
    return 0;
}

