
PROGRAMS = main \
		   monitor_server

LDLIBS = -lpthread

all: $(PROGRAMS)

debug: LDLIBS += -DDEBUG -g
debug: all


O1: LDLIBS += -O1
O1: all

O2: LDLIBS += -O2
O2: all

O3: LDLIBS += -O3
O3: all

clean:
	rm -f $(PROGRAMS)