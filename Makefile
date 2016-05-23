CFLAGS=-g -O0
LDFLAGS=-pthread

all: ToBServer ToBHotspot

ToBServer ToBHotspot: ToB.h

clean:
	rm -f ToBServer ToBHotspot
