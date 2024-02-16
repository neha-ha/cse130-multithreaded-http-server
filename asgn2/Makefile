EXECBIN  = httpserver
#SOURCES  = $(wildcard *.c) #wildcard includes all source files in the directory with the .c extension-- in this case, memory.c
SOURCES = httpserver.c 
OBJECTS  = $(SOURCES:%.c=%.o) # all the .o files from the corresponding .c files in SOURCES
FORMATS  = $(SOURCES:%.c=%.fmt)

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Werror -Wextra -Wpedantic -Wstrict-prototypes

.PHONY: all clean format

all: $(EXECBIN)
regex: regextest

$(EXECBIN): $(OBJECTS) asgn2_helper_funcs.a
	$(CC) -o $@ $^ 
# -o $@ $^ means 'output' to the 'target file' (what's left of :, execbin in this case) the 'dependencies' (what's right of :, the objects, etc)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

regextest: regextest.o
	$(CC) -o $@ $^ 


clean:
	rm -f $(EXECBIN) $(OBJECTS)

format: $(FORMATS)

%.fmt: %.c
	$(FORMAT) -i $<
	touch $@
