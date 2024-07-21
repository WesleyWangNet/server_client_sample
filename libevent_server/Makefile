TARGET:=server

SRCS:=$(wildcard *.c)
OBJS:=$(patsubst %.c, %.o, $(SRCS))
LDFLAGS:=-L/lib/x86_64-linux-gnu  -levent_extra  -levent_pthreads -levent_openssl -levent_core -levent
CFLAGS:=-I/usr/include

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) -o $@ -c $^ 

clean:
	rm -f $(TARGET)
	rm -f *.o
