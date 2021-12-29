
PROGRAMS = main \
		   monitor_server

LDLIBS = -lpthread

all: $(PROGRAMS)

debug: LDLIBS += -DDEBUG -g
debug: all

clean:
	rm -f $(PROGRAMS)