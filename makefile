CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -ffast-math

TARGET = mandelbrot_seq

OBJS = main.o mandelbrot.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c mandelbrot.h
	$(CC) $(CFLAGS) -c main.c

mandelbrot.o: mandelbrot.c mandelbrot.h
	$(CC) $(CFLAGS) -c mandelbrot.c

clean:
	rm -f *.o $(TARGET) mandelbrot.bin mandelbrot.pgm