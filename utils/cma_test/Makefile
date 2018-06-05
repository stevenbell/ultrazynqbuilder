APP = test_cma

# Add any other object files to this list below
APP_OBJS = cma.o test_cma.o

all: build

build: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) *.o

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

