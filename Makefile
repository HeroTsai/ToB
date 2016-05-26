CFLAGS=-g -O0 -Wall
LDFLAGS=-pthread

all: ToBServer ToBHotspot

ToBServer ToBHotspot: ToB.h

clean:
	rm -f ToBServer ToBHotspot
