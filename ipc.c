#include "ipc.h"

// Deklaracje zmiennych globalnych
//int shm_id;
Sklep *sklep;
int semID;

// Funkcja do tworzenia/dołączania pamięci dzielonej
Sklep *get_shared_memory() {
    // Tworzenie lub dołączanie się do segmentu pamięci dzielonej
    int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0600 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Błąd tworzenia/dołączania segmentu pamięci dzielonej");
        return NULL;
    }

    // Dołączanie segmentu pamięci dzielonej
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
        // Odłączenie segmentu pamięci dzielonej
        if (shmdt(sklep) == -1) {
            perror("Błąd odłączania segmentu pamięci dzielonej");
        }

        // Usunięcie segmentu pamięci dzielonej
        int shm_id = shmget(SHM_KEY, sizeof(Sklep), 0600);
        if (shm_id != -1) {
            if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
                perror("Błąd usuwania pamięci dzielonej");
            } else {
                ("Pamięć dzielona została poprawnie usunięta\n");
            }
        }
    }
}

int create_semaphore() {
    int semID = semget(SEM_KEY, 2, IPC_CREAT | IPC_EXCL | 0600);
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
        perror("error semop P");
        return -1;
    }
    return 0;
}

int semaphore_v(int semID, int number) {
    struct sembuf bufor_sem = {number, 1, 0};
    if (semop(semID, &bufor_sem, 1) == -1) {
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
