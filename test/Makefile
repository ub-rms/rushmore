.PHONY: all clean

ifndef LIBRUSHMORE
$(error Please specify LIBRUSHMORE. "$$ export LIBRUSHMORE=<rushmore_soruce>/librushmore")
endif


CFLAGS+=-I$(LIBRUSHMORE)/include
LDFLAGS+=-L$(LIBRUSHMORE)/ -lrushmore -lpthread

PROG=run_test
SRCS=run_test.c

$(PROG): $(SRCS) | $(LIBRUSHMORE)/librushmore.a
	arm-linux-gnueabihf-gcc -static -march=armv7-a $(CFLAGS) $^ $(LDFLAGS) -o $@


clean:
	rm -f $(PROG) $(PROG_NW)
