all: splashscreen_rpi4

.PHONY: all

splashscreen_rpi4: splashscreen_rpi4.c
	# It's a small application, make sure to warn about all strange things.
	gcc -o $@ $@.c -ldrm -lgbm -lEGL -lGLESv2 -I/usr/include/libdrm -I/usr/include/GLES2 -Wall ${CXXFLAGS}

run: splashscreen_rpi4
	./splashscreen_rpi4
