all: splashscreen_rpi4

.PHONY: all

splashscreen_rpi4: splashscreen_rpi4.c
	gcc -o $@ $@.c -ldrm -lgbm -lEGL -lGLESv2 -I/usr/include/libdrm -I/usr/include/GLES2 -Wall ${CXXFLAGS}
