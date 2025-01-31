#include "ipc.h"

// === KLUCZ ===
key_t create_key(const char *path, int id) {
    key_t key = ftok(path, id);
    if (key == -1) {
        perror("error ftok");
        exit(EXIT_FAILURE);
    }
    return key;
}

// === SEMAFORY ===
int create_semaphore(key_t key, int number, int flags) {
    int semID = semget(key, number, IPC_CREAT | IPC_EXCL | flags);
    if (semID == -1) {
        if (errno == EEXIST) {
            semID = semget(key, number, flags);
            if (semID == -1) {
                perror("error semget (existing)");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("error semget");
            exit(EXIT_FAILURE);
        }
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

void print_semaphore_values(int semID, int number) {
    unsigned short values[number];
    if (semctl(semID, 0, GETALL, values) == -1) {
        perror("error semctl GETALL");
        exit(EXIT_FAILURE);
    }

    printf("Stan semaforów (semID: %d):\n", semID);
    for (int i = 0; i < number; i++) {
        printf("  Semafor %d: %d\n", i, values[i]);
    }
}

// === PAMIĘĆ DZIELONA ===
int create_shared_memory(key_t key, size_t size, int flags) {
    int shmID = shmget(key, size, IPC_CREAT | IPC_EXCL | flags);
    if (shmID == -1) {
        if (errno == EEXIST) {
            shmID = shmget(key, size, flags);
            if (shmID == -1) {
                perror("error shmget (existing)");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("error shmget");
            exit(EXIT_FAILURE);
        }
    }
    return shmID;
}

void *attach_shared_memory(int shmID, int shmflg) {
    void *shmaddr = shmat(shmID, NULL, shmflg);
    if (shmaddr == (void *)-1) {
        perror("error shmat");
        exit(EXIT_FAILURE);
    }
    return shmaddr;
}

void detach_shared_memory(const void *shmaddr) {
    if (shmdt(shmaddr) == -1) {
        perror("error shmdt");
        exit(EXIT_FAILURE);
    }
}

void remove_shared_memory(int shmID) {
    if (shmctl(shmID, IPC_RMID, NULL) == -1) {
        perror("error shmctl IPC_RMID");
        exit(EXIT_FAILURE);
    }
}

// === KOLEJKA KOMUNIKATÓW ===
int create_message_queue(key_t key, int flags) {
    int msgID = msgget(key, IPC_CREAT | IPC_EXCL | flags);
    if (msgID == -1) {
        if (errno == EEXIST) {
            msgID = msgget(key, flags);
            if (msgID == -1) {
                perror("error msgget (existing)");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("error msgget");
            exit(EXIT_FAILURE);
        }
    }
    return msgID;
}

void send_message(int msgID, const void *msg, size_t size, int flags) {
    if (msgsnd(msgID, msg, size, flags) == -1) {
        perror("error msgsnd");
        exit(EXIT_FAILURE);
    }
}

ssize_t receive_message(int msgID, void *msg, size_t size, long msgtype, int flags) {
    ssize_t received = msgrcv(msgID, msg, size, msgtype, flags);
    if (received == -1) {
        perror("error msgrcv");
        exit(EXIT_FAILURE);
    }
    return received;
}

void remove_message_queue(int msgID) {
    if (msgctl(msgID, IPC_RMID, NULL) == -1) {
        perror("error msgctl IPC_RMID");
        exit(EXIT_FAILURE);
    }
}

