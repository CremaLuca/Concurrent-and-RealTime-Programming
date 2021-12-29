
PROGRAMS = main \
		   monitor_server

LDLIBS = -lpthread

all: $(PROGRAMS)

clean:
	rm -f $(PROGRAMS)