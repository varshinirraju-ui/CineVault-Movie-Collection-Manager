# CineVault - build with `make`, then run `./cinevault` (or `cinevault.exe` on Windows).
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
SRC      = server.c movies.c

ifeq ($(OS),Windows_NT)
  TARGET = cinevault.exe
  LIBS   = -lws2_32
else
  TARGET = cinevault
  LIBS   =
endif

$(TARGET): $(SRC) movies.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f cinevault cinevault.exe data/movies.dat.tmp

.PHONY: run clean
