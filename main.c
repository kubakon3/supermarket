#include <sys/types.h>
#include <sys/wait.h>
#include "ipc.h"

#define MAX_KASY 10

key_t shm_key, sem_key, msg_key;
int shm_id, sem_id, msg_id;
int *liczba_klientow;

// Inicjalizacja mechanizmów IPC
void inicjalizuj_ipc() {
    shm_key = create_key(".", 'a');
    sem_key = create_key(".", 'b');
    msg_key = create_key(".", 'c');
    
    shm_id = create_shared_memory(shm_key, sizeof(int), 0600);
    liczba_klientow = (int *)attach_shared_memory(shm_id, 0);
    *liczba_klientow = 0;

    sem_id = create_semaphore(sem_key, 1, 0600);
    set_semaphore(sem_id, 0, 1);

    msg_id =  create_message_queue(msg_key, 0600);
}

// Uruchomienie procesów kierownika, strażaka i klientów
void uruchom_procesy() {
    pid_t kierownik_pid = fork();
    if (kierownik_pid == 0) {
        execl("./kierownik_kasjerow", "kierownik_kasjerow", NULL);
        perror("Blad uruchamiania kierownika");
        exit(1);
    }

    pid_t strazak_pid = fork();
    if (strazak_pid == 0) {
        execl("./strazak", "strazak", NULL);
        perror("Blad uruchamiania strażaka");
        exit(1);
    }

    while (1) {
        sleep(rand() % 3 + 1);
        pid_t klient_pid = fork();
        if (klient_pid == 0) {
            execl("./klient", "klient", NULL);
            perror("Blad uruchamiania klienta");
            exit(1);
        }
    }
}

int main() {
    inicjalizuj_ipc();
    uruchom_procesy();
    
    printf("Koniec programu.\n");
    return 0;
}

