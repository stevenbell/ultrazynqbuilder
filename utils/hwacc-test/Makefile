CC		= aarch64-linux-gnu-gcc
CFLAGS	= -std=c99 -g -Wall

TARGETS	= oneinput twoinput
DEPS	= cma.o

all: $(TARGETS)

$(TARGETS): $(DEPS)
	$(CC) -c  $(CFLAGS) $@.c -o $@.o
	$(CC) $(CFLAGS) $@.o $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGETS) *.o
