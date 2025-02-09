#ifndef STRAZAK_H
#define STRAZAK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

// Deklaracje funkcji
void sigusr_handler(int sig);
void *strazak(void *arg);

#endif // STRAZAK_H
