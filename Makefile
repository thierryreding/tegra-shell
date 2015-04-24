CC = $(CROSS_COMPILE)gcc
CFLAGS = -D_GNU_SOURCE -O0 -ggdb -Wall -Werror $(EXTRA_CFLAGS) $(shell pkg-config --cflags libxml-2.0)
LIBS = $(shell pkg-config --libs libxml-2.0)

objs = main.o chip.o

tegra-shell: $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(objs): %.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o tegra-shell $(objs)
