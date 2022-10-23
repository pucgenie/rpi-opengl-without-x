CC ?= gcc
LIBS = -ldrm -lgbm -lEGL -lGLESv2
INCLUDES = -I/usr/include/libdrm -I/usr/include/GLES2

all: yafmtc splashscreen_rpi4

clean:
	rm -f splashscreen_rpi4 splashscreen_rpi4-debug *.o

.PHONY: all

%.o: %.c
	$(CC) -o $@ -c $^ $(INCLUDES) -g0 ${CFLAGS}

splashscreen_rpi4: splashscreen_rpi4.o yafmtc.o
	$(CC) -o $@ $^ $(LIBS) -g0 ${CFLAGS}

splashscreen_rpi4-debug: splashscreen_rpi4.c
	# It's a small application, make sure to warn about all strange things.
	$(CC) -o $@ $^ $(LIBS) $(INCLUDES) -Wall -g

run: splashscreen_rpi4
	./splashscreen_rpi4
