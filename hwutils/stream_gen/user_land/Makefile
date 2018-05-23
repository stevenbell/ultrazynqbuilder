APP = stream-gen-app

# Add any other object files to this list below
APP_OBJS = stream-gen.o main.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

