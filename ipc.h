#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>

// === KLUCZ ===
key_t create_key(const char *path, int id);

// === SEMAFORY ===
int create_semaphore(key_t key, int number, int flags);
void set_semaphore(int semID, int number, int value);
int semaphore_p(int semID, int number);
int semaphore_v(int semID, int number);
void remove_semaphore(int semID);
void print_semaphore_values(int semID, int number);

// === PAMIĘĆ DZIELONA ===
int create_shared_memory(key_t key, size_t size, int flags);
void *attach_shared_memory(int shmID, int shmflg);
void detach_shared_memory(const void *shmaddr);
void remove_shared_memory(int shmID);

// === KOLEJKA KOMUNIKATÓW ===
int create_message_queue(key_t key, int flags);
void send_message(int msgID, const void *msg, size_t size, int flags);
ssize_t receive_message(int msgID, void *msg, size_t size, long msgtype, int flags);
void remove_message_queue(int msgID);

#endif

