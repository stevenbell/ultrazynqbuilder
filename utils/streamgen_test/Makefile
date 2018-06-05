APP = test_stream

# Add any other object files to this list below
APP_OBJS = stream_gen.o test_stream.o

all: build

build: $(APP)

.PHONY: clean
clean:
	rm -f $(APP) *.o

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

