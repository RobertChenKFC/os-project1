CC=gcc
LIBS=-lpthread
DBG=-g -DDEBUG

all: main.c
	$(CC) -o main main.c $(LIBS) -O2
debug: main.c
	$(CC) -o main main.c $(LIBS) $(DBG)
theory: theory.c
	$(CC) -o theory theory.c -O2
theory-debug: theory.c
	$(CC) -o theory theory.c $(DBG)
clean:
	rm -rf main theory

