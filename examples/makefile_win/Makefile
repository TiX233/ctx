CC := gcc
CFLAGS := -Wall -std=c99 -g -O0
CPPFLAGS := -I. -Ictx -Iltx
TARGET := main.exe
SRC := main.c ctx/ctx.c ltx/ltx.c
OBJ := $(SRC:.c=.o)
DEPS := $(wildcard *.h ctx/*.h ltx/*.h)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^

%.o: %.c $(DEPS)
	@echo "check" $<
	@python ctx/coro_translater.py $< --line
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)
