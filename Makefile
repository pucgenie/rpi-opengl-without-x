CC ?= gcc
LD ?= ld
LIBS = -ldrm -lgbm -lEGL -lGLESv2
#LIBS = drm gbm EGL GLESv2
INCLUDES = -I/usr/include/libdrm -I/usr/include/GLES2

all: yafmtc.o splashscreen_rpi4

clean:
	rm -f splashscreen_rpi4 splashscreen_rpi4-debug *.o *.glslo

.PHONY: all

%.o: %.c
	$(CC) -flto -o $@ -c $< $(INCLUDES) -g3 ${CFLAGS}

%.glslo: %.glsl
	# TODO: precompile, not add just the script
	$(LD) -r -b binary -o $@ $^

splashscreen_rpi4: splashscreen_rpi4.o yafmtc.o vertexShader1.glslo fragmentShader1.glslo
	# $(LD) -o $@ "$(shell gcc --print-file-name=crtbegin.o)" "$(shell gcc --print-file-name=crtend.o)" $^ -lc $(LIBS)
	$(CC) -v -o $@ $^ $(LIBS)

run: splashscreen_rpi4
	./splashscreen_rpi4
