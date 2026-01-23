#include "ipc.h"

// Deklaracje zmiennych globalnych
Sklep *sklep;
int semID;
int aktywne_kasy[MAX_KASY] = {0};

// Funkcja do tworzenia/dołączania pamięci dzielonej
Sklep *get_shared_memory() {
    int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0600 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia/dołączania segmentu pamięci dzielonej");
        return NULL;
    }

    Sklep *shared_memory = (Sklep *)shmat(shm_id, NULL, 0);
    if (shared_memory == (Sklep *)-1) {
        perror("Błąd dołączania segmentu pamięci dzielonej");
        return NULL;
    }

    return shared_memory;
}

// Funkcja do usuwania pamięci dzielonej
void destroy_shared_memory(Sklep *sklep) {
    if (sklep != NULL) {
        if (shmdt(sklep) == -1) {
            perror("Błąd odłączania segmentu pamięci dzielonej");
        }

        int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0600);
        if (shm_id != -1) {
            if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
                perror("Błąd usuwania pamięci dzielonej");
            } else {
                printf("Pamięć dzielona została poprawnie usunięta\n");
            }
        }
    }
}

int get_queue_id(int index) {
    if (semaphore_p(semID, 0) == -1) { 
        perror("Błąd semaphore_p w get_queue_id");
        return -1;
    }

    int queue_id = sklep->kolejki_kas[index];

    if (semaphore_v(semID, 0) == -1) { 
        perror("Błąd semaphore_v w get_queue_id");
        return -1;
    }

    return queue_id;
}

int get_cashiers() {
    if (semaphore_p(semID, 0) == -1) {
        perror("Błąd semaphore_p w get_cashiers");
        return -1;
    }

    int cashiers = sklep->liczba_kas;

    if (semaphore_v(semID, 0) == -1) {
        perror("Błąd semaphore_v w get_cashiers");
        return -1;
    }

    return cashiers;
}

int get_active_cashier(int index) {
    if (semaphore_p(semID, 2) == -1) { 
        perror("Błąd semaphore_p w get_active_cashier");
        return -1;
    }

    int flag = aktywne_kasy[index];

    if (semaphore_v(semID, 2) == -1) { 
        perror("Błąd semaphore_v w get_active_cashier");
        return -1;
    }

    return flag;
}

void set_active_cashier(int index) {
    if (semaphore_p(semID, 2) == -1) { 
        perror("Błąd semaphore_p w get_active_cashier");
    }

    aktywne_kasy[index] = 1;

    if (semaphore_v(semID, 2) == -1) { 
        perror("Błąd semaphore_v w get_active_cashier");
    }
}

void set_inactive_cashier(int index) {
    if (semaphore_p(semID, 2) == -1) { 
        perror("Błąd semaphore_p w get_active_cashier");
    }

    aktywne_kasy[index] = 0;

    if (semaphore_v(semID, 2) == -1) { 
        perror("Błąd semaphore_v w get_active_cashier");
    }
}

int check_fire_flag(Sklep *sklep) {    
    if (semaphore_p(semID, 0) == -1) {
        perror("Błąd semaphore_p w sygnale");
        return -1;
    }
    
    int flag = sklep->fire_flag;
    
    if (semaphore_v(semID, 0) == -1) {
        perror("Błąd semaphore_v w sygnale");
        return -1;
    }
    
    return flag;
}
  

int create_semaphore() {
    int semID = semget(SEM_KEY, 6, IPC_CREAT | 0600);
    if (semID == -1) {
        perror("Błąd tworzenia semafora");
        return -1;
    }
    return semID;
}

void set_semaphore(int semID, int number, int value) {
    if (semctl(semID, number, SETVAL, value) == -1) {
        perror("error semctl SETVAL");
        exit(EXIT_FAILURE);
    }
}

int semaphore_p(int semID, int number) {
    struct sembuf bufor_sem = {number, -1, 0};
    if (semop(semID, &bufor_sem, 1) == -1) {
        // if (errno == EINTR) {
        //     // Przerwane przez sygnał (np. SIGCHLD) - ponów próbę
        //     continue;
        // }
        perror("error semop P");
        return -1;
    }
    return 0;
}

int semaphore_v(int semID, int number) {
    struct sembuf bufor_sem = {number, 1, 0};
    if (semop(semID, &bufor_sem, 1) == -1) {
        // if (errno == EINTR) {
        //     // Przerwane przez sygnał (np. SIGCHLD) - ponów próbę
        //     continue;
        // }
        perror("error semop V");
        return -1;
    }
    return 0;
}

void remove_semaphore(int semID) {
    if (semctl(semID, 0, IPC_RMID) == -1) {
        perror("error semctl IPC_RMID");
        exit(EXIT_FAILURE);
    }
}
