CC ?= gcc
LIBS = -ldrm -lgbm -lEGL -lGLESv2
INCLUDES = -I/usr/include/libdrm -I/usr/include/GLES2

all: yafmtc.o splashscreen_rpi4

clean:
	rm -f splashscreen_rpi4 splashscreen_rpi4-debug *.o

.PHONY: all

%.o: %.c
	$(CC) -o $@ -c $^ $(INCLUDES) ${CFLAGS}

splashscreen_rpi4: splashscreen_rpi4.o yafmtc.o
	$(CC) -o $@ $^ $(LIBS) ${CFLAGS}

run: splashscreen_rpi4
	./splashscreen_rpi4
