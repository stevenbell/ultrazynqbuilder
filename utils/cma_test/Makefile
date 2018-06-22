CC	= aarch64-linux-gnu-gcc
APP = test_cma

APP_SRC		= $(wildcard *.c);
APP_OBJS	= $(APP_SRC:.c=.o) 

all: build

build: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) *.o

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

