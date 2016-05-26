CFLAGS=-g -O0 -Wall
LDFLAGS=-pthread

all: ToBServer ToBHotspot ToBHotspot_aarch64

ToBServer ToBHotspot: ToB.h

ToBHotspot_aarch64: ToBHotspot.c ToB.h
	aarch64-linux-gnu-gcc-5 $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f ToBServer ToBHotspot ToBHotspot_aarch64
