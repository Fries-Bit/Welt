CC = gcc
CFLAGS = -Wall
SRC = src/main.c src/fsal/fsal.c
TARGET = welt

ifeq ($(OS),Windows_NT)
    TARGET = welt.exe
    RM = del /Q
else
    RM = rm -f
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	$(RM) $(TARGET) a.exe

.PHONY: all clean
