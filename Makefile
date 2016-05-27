CFLAGS=-g -O0 -Wall
LDFLAGS=-pthread
CC_AARCH64=aarch64-linux-gnu-gcc
BINS=ToBServer ToBHotspot ToBHotspot_aarch64 rssid rssi rssi_aarch64

all: $(BINS)

ToBServer ToBHotspot rssid rssi: ToB.h

ToBHotspot_aarch64: ToBHotspot.c ToB.h
	$(CC_AARCH64) $(CFLAGS) $(LDFLAGS) $^ -o $@

rssi_aarch64: rssi.c ToB.h
	$(CC_AARCH64) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(BINS)
