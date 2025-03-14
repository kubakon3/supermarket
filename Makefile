# Makefile dla projektu supermarket

# Kompilator
CC = gcc

# Flagi kompilacji
CFLAGS = -Wall -Wextra -lpthread -lrt

# Pliki obiektowe
OBJS = ipc.o strazak.o

# Cel domyślny
all: main kierownik_kasjerow klient

# Kompilacja plików obiektowych
ipc.o: ipc.c ipc.h
	$(CC) -c ipc.c -o ipc.o $(CFLAGS)

strazak.o: strazak.c strazak.h
	$(CC) -c strazak.c -o strazak.o $(CFLAGS)

# Kompilacja głównego programu
main: main.c $(OBJS)
	$(CC) -o main main.c $(OBJS) $(CFLAGS)
	chmod 700 main  # Ustawienie uprawnień 700

# Kompilacja programu kierownika kasjerów
kierownik_kasjerow: kierownik_kasjerow.c ipc.o
	$(CC) -o kierownik_kasjerow kierownik_kasjerow.c ipc.o $(CFLAGS)
	chmod 700 kierownik_kasjerow  # Ustawienie uprawnień 700

# Kompilacja programu klienta
klient: klient.c ipc.o
	$(CC) -o klient klient.c ipc.o $(CFLAGS)
	chmod 700 klient  # Ustawienie uprawnień 700

# Czyszczenie plików obiektowych i wykonywalnych
clean:
	rm -f *.o main kierownik_kasjerow klient

# Phony targets
.PHONY: all clean
