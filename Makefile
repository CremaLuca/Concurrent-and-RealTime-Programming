
PROGRAMS = main \
		   monitor_server

LDLIBS = -lpthread

all: $(PROGRAMS)

debug: LDLIBS += -DDEBUG -g
debug: all


o1: LDLIBS += -o1
o1: all

o2: LDLIBS += -o2
o2: all

o3: LDLIBS += -o3
o3: all

clean:
	rm -f $(PROGRAMS)