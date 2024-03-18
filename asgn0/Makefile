CC=clang
hello:   CFLAGS=-Wall -Wextra -Wstrict-prototypes -Werror -pedantic -gdwarf-4
LDFLAGS=-lm

EXECBIN=hello
OBJS=\
    hello.o 
	
.PHONY: all clean

all: hello

$(EXECBIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXECBIN) $(OBJS)

format:
	clang-format -i --style=file *.[ch]
