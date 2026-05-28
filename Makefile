CFLAGS = -Wall -Wextra -O3 -flto
LDFLAGS = -lpthread -flto

.PHONY: all clean install

all: yacfs-metrics

yacfs-metrics: src/metrics.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f yacfs-metrics

install: yacfs-metrics
	cp yacfs-metrics /usr/local/bin/
