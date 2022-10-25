// gcc -o triangle_rpi4 triangle_rpi4.c -ldrm -lgbm -lEGL -lGLESv2 -I/usr/include/libdrm -I/usr/include/GLES2
// super simple stand-alone example of using DRM/GBM+EGL without X11 on the pi4

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// fwrite
#include <stdio.h>

#include <assert.h>
#include <errno.h>

// inline whole functionality
#include "yafmtc.h"
int writeToStderr(const char * const str_sans_zero, const size_t len) {
	return fwrite(str_sans_zero, len, 1, stderr);
}
// pucgenie: Let's reinvent the wheel again.
//#include "yafmtn.c"
//int writeToStderr2(const struct nstring * part) {
//    do {
//        int ret = fwrite(part->str, part->len, 1, stderr);
//        if (ret == -1) {
//return -1;
//        }
//    } while ((part = part.link) != NULL);
//    return 0;
//}
//#define fmtErrln(fmtStr, ...) ffmt(#fmtStr "\n", writeToStderr, (const char * const []) { __VA_ARGS__ })
#define fmtErr(fmtStr, ...) ffmt(fmtStr, writeToStderr, (const char * const []) { __VA_ARGS__ })

//#define SPLASHSCREEN_TRACE(x) x
#define SPLASHSCREEN_TRACE(x)

// The following code related to DRM/GBM was adapted from the following sources:
// https://github.com/eyelash/tutorials/blob/master/drm-gbm.c
// and
// https://www.raspberrypi.org/forums/viewtopic.php?t=243707#p1499181
//
// I am not the original author of this code, I have only modified it.
// pucgenie: Me too.

// File descriptor Index type
typedef int fdI_t;
// Connector Id type
typedef uint32_t connectorI_t;

static struct gbm_device *gbmDevice;
static struct gbm_surface *gbmSurface;
// platform-independent. Can't access members directly.
static struct gbm_bo *bo = NULL;
static uint32_t fb;

static const char *const eglGetErrorStr(); // moved to bottom


static void gbmSwapBuffers(
	  const fdI_t device
	, EGLDisplay const display
	, EGLSurface const surface
	, drmModeModeInfoPtr const mode
	, connectorI_t *const connectorId
	, drmModeCrtcPtr const crtc
	) {
	SPLASHSCREEN_TRACE(fputs("Before egl swap buffers\n", stderr));
	eglSwapBuffers(display, surface);
	if (bo != NULL) {
		fputs("bo != NULL\n", stderr);
		drmModeRmFB(device, fb);
		// pucgenie: TODO: Reuse buffer? We can assume that the size doesn't change.
		gbm_surface_release_buffer(gbmSurface, bo);
	}
	SPLASHSCREEN_TRACE(fputs("Before gbm lock buffer\n", stderr));
	bo = gbm_surface_lock_front_buffer(gbmSurface);
	uint32_t handle = gbm_bo_get_handle(bo).u32;
	uint32_t pitch = gbm_bo_get_stride(bo);
	
	SPLASHSCREEN_TRACE(fputs("Before drm add FB\n", stderr));
	// pucgenie: Are you sure that the FB has to be initialized before setting Crtc?
	drmModeAddFB(device, mode->hdisplay, mode->vdisplay, 24, 32, pitch, handle, &fb);

	if (fb == 0 || fb == -1) {
		fputs("No FB created??\n", stderr);
	}
	
	SPLASHSCREEN_TRACE(fputs("Before drm set crtc\n", stderr));
	drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, connectorId, 1, mode);
}

static void gbmClean(
	  const fdI_t device
	, struct _drmModeCrtc *const crtc
	, connectorI_t * const connectorId
	) {
	// set the previous crtc
	drmModeSetCrtc(device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, connectorId, 1, &crtc->mode);
	drmModeFreeCrtc(crtc);

	if (bo != NULL) {
		drmModeRmFB(device, fb);
		gbm_surface_release_buffer(gbmSurface, bo);
	}

	gbm_surface_destroy(gbmSurface);
	gbm_device_destroy(gbmDevice);
}

// The following are GLSL shaders for rendering a triangle on the screen
#define STRINGIFY(x) #x

// linked precompiled shader
extern const char *const _binary_vertexShader1_start;
extern const char *const _binary_vertexShader1_end;

extern const char *const _binary_fragmentShader1_start;
extern const char *const _binary_fragmentShader1_end;

int main(int argc, char* argv[]) {

	static fdI_t device;
	static drmModeCrtc *crtc;
	static uint32_t connectorId;
	static drmModeModeInfo mode;

	{
		//const char * const TRY_CARDS[] = {
		//	"/dev/dri/card1",
		//	"/dev/dri/card0",
		//	NULL,
		//};
		//const char * const * currentCard = TRY_CARDS;

		//fputs("unavailable: GL_ARB_gl_spirv GL_SHADER_BINARY_FORMAT_SPIR_V_ARB\n", stderr);
		

		//stat(currentCard, statBuf);
		// we have to try card0 and card1 to see which is valid. fopen will work on both, so...
		//device = open(*currentCard, O_RDWR | O_CLOEXEC);
		// https://gist.github.com/robertkirkman/641ffd147157cc35eba7b404144228ce/revisions Linux_DRM_OpenGLES.c
		// name = "vc4" "v3d"
		if (argc == 1) {
			argv[1] = "vc4";
		}
		device = drmOpenWithType(argv[1], 0, DRM_NODE_PRIMARY);
		if (device == -1) {
			fprintf(stderr, "open_errno: %d\n", errno);
return EXIT_FAILURE;
		}

		drmModeRes *resources = drmModeGetResources(device);
//		while (resources == NULL) {
//			// if we have the right device we can get it's resources
//			
//			fmtErr("no_drm_resources: \"$0\"\n", *currentCard);
//
//			currentCard++;
//			if (*currentCard == NULL) {
//				// handle error after loop
//				break;
//			}
//			device = open(*currentCard, O_RDWR | O_CLOEXEC); // if not, try the other one: (1)
//			resources = drmModeGetResources(device);
//		}
//		fmtErr("currentCard: \"$0\"\n", *currentCard);
		

		if (true) {
			drmVersionPtr version;
			version = drmGetVersion(device);
			if (version != NULL) {
				fputs("drm_version_name: ", stdout);
				fputs(version->name, stdout);
				fputc('\n', stdout);
				drmFreeVersion(version);
			} else {
				fputs("Couldn't determine drm version (name of driver).\n", stderr);
			}
		}
		if (resources == NULL) {
			fputs("error: no_drm_resources\n", stderr);
return EXIT_FAILURE;
		}

		drmModeConnector *connector = NULL;
		assert(resources->count_connectors > 0);
		{
			int i;// = resources->count_connectors;
			//while (i --> 0) {
			for (i = 0; i < resources->count_connectors; ++i) {
				drmModeConnector *connector_tmp = drmModeGetConnector(device, resources->connectors[i]);
				if (connector_tmp->connection == DRM_MODE_CONNECTED) {
					connector = connector_tmp;
					// Don't break, print other connectors too
					if (false) {
						break;
					}
				} else {
					fprintf(stderr, "{alt_connector_id: %i, connection_state: %d, count_modes: %d}\n"
						, i, connector_tmp->connection, connector_tmp->count_modes);
					drmModeFreeConnector(connector_tmp);
				}
			}
			if (connector == NULL && (i < 0 || i >= resources->count_connectors)) {
				fputs("Unable to get connector\n", stderr);
				drmModeFreeResources(resources);
return EXIT_FAILURE;
			}
		}
		connectorId = connector->connector_id;
		// copy: array of resolutions and refresh rates supported by this display
		mode = connector->modes[0];
		fprintf(stderr, "resolution: %ix%i\n", mode.hdisplay, mode.vdisplay);

		drmModeEncoder *encoder = NULL;
		if (!connector->encoder_id) {
			fputs("Connector didn't define an encoder_id, searching for myself...\n", stderr);
			fprintf(stderr, "Count encoders: %d\n", connector->count_encoders);
			// https://gist.github.com/robertkirkman/641ffd147157cc35eba7b404144228ce :574
			for (size_t j=0; j<connector->count_encoders; j++) {
				encoder = drmModeGetEncoder(device, connector->encoders[j]);
				switch (encoder->encoder_type) {
					case DRM_MODE_ENCODER_VIRTUAL:
						fputs("Encoder type DRM_MODE_ENCODER_VIRTUAL\n", stderr);
					break;
					case DRM_MODE_ENCODER_TMDS:
						fputs("Encoder type DRM_MODE_ENCODER_TMDS\n", stderr);
					break;
					default:
						fprintf(stderr, "Encoder type 0x%x\n", encoder->encoder_type);
					break;
				}
				/* find the first valid CRTC if not assigned */
				if (!encoder->crtc_id) {
					fputs("Encoder didn't define a crtc_id, searching for myself...\n", stderr);
					for (size_t k = 0; k < resources->count_crtcs; ++k) {
						/* check whether this CRTC works with the encoder */
						if (!(encoder->possible_crtcs & (1 << k)))
							continue;

						encoder->crtc_id = resources->crtcs[k];
						break;
					}

					if (!encoder->crtc_id) {
						printf("Encoder(%d): no possible CRTC found!\n", encoder->encoder_id);
						drmModeFreeEncoder(encoder);
						encoder = NULL;
						continue;
					} else {
						fputs("Possibly matching Crtc found at 2nd attempt!\n", stderr);
					}
				} else {
					fputs("Possibly matching Crtc found at 1st attempt!\n", stderr);
				}
				//connector->encoder_id = encoder->encoder_id;
				fputs("Not freeing found alternative encoder.\n", stderr);
				//drmModeFreeEncoder(encoder);
			}
		} else {
			encoder = drmModeGetEncoder(device, connector->encoder_id);
		}
		if (encoder == NULL) {
			drmModeFreeConnector(connector);
			drmModeFreeResources(resources);
return EXIT_FAILURE;
		}

		crtc = drmModeGetCrtc(device, encoder->crtc_id);
		drmModeFreeEncoder(encoder);
		drmModeFreeConnector(connector);
		drmModeFreeResources(resources);
	}
	gbmDevice = gbm_create_device(device);
	gbmSurface = gbm_surface_create(gbmDevice, mode.hdisplay, mode.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	static EGLDisplay display;
	display = eglGetDisplay(gbmDevice);

	{
		int major, minor;

		if (eglInitialize(display, &major, &minor) == EGL_FALSE) {
			fmtErr("eglInitialize_error: $0\n", eglGetErrorStr());
			
			eglTerminate(display);
			gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
		}

		// Make sure that we can use OpenGL in this EGL app.
		eglBindAPI(EGL_OPENGL_API);

		fprintf(stderr, "Initialized EGL version: %d.%d\n", major, minor);
	}

	GLint posLoc, colorLoc;
	//EGLConfig *configs = malloc(count * sizeof(EGLConfig));
	static EGLContext context;
	static EGLSurface surface;
	{
		// We will use the screen resolution as the desired width and height for the viewport.
		EGLint count;
		eglGetConfigs(display, NULL, 0, &count);
		{
			EGLConfig configs[count];
			// The following code was adopted from
			// https://github.com/matusnovak/rpi-opengl-without-x/blob/master/triangle.c
			// and is licensed under the Unlicense.
			const EGLint configAttribs[] = {
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_DEPTH_SIZE, 8,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
				EGL_NONE};
			EGLint numConfigs;
			if (!eglChooseConfig(display, configAttribs, configs, count, &numConfigs)) {
				fmtErr("eglChooseConfig_error: $0\n", eglGetErrorStr());
				eglTerminate(display);
				gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
			}

			// I am not exactly sure why the EGL config must match the GBM format.
			// But it works!
			EGLint configIndex = numConfigs;
			// pucgenie: configIndex ISN'T unsigned so let's just use it...
			for (EGLint id; configIndex --> 0;) {
				if (!eglGetConfigAttrib(display, configs[configIndex], EGL_NATIVE_VISUAL_ID, &id)) {
			continue;
				}
				if (id == GBM_FORMAT_XRGB8888) {
			break;
				}

			}
			// pucgenie: ... for error checking.
			if (configIndex == -1) {
				fmtErr("eglGetConfigAttrib_no_GBM_FORMAT_XRGB8888: $0\n", eglGetErrorStr());
				eglTerminate(display);
				gbm_surface_destroy(gbmSurface);
				gbm_device_destroy(gbmDevice);
return EXIT_FAILURE;
			}
			const EGLint contextAttribs[] = {
				EGL_CONTEXT_CLIENT_VERSION, 2,
				EGL_NONE};
			context = eglCreateContext(display, configs[configIndex], EGL_NO_CONTEXT, contextAttribs);
			if (context == EGL_NO_CONTEXT) {
				fmtErr("eglCreateContext_EGL_NO_CONTEXT: $0\n", eglGetErrorStr());
				eglTerminate(display);
				gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
			}

			surface = eglCreateWindowSurface(display, configs[configIndex], gbmSurface, NULL);
			if (surface == EGL_NO_SURFACE) {
				fmtErr("eglCreateWindowSurface_EGL_NO_SURFACE: $0\n", eglGetErrorStr());
				eglDestroyContext(display, context);
				eglTerminate(display);
				gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
			}
		}
		eglMakeCurrent(display, surface, surface, context);

	}
	// Set GL Viewport size, always needed!
	glViewport(0, 0, mode.hdisplay, mode.vdisplay);
	{
		// Get GL Viewport size and test if it is correct.
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		// viewport[2] and viewport[3] are viewport width and height respectively
		fprintf(stderr, "GL Viewport size: %dx%d\n", viewport[2], viewport[3]);

		if (viewport[2] != mode.hdisplay || viewport[3] != mode.vdisplay) {
			fputs("Error! The glViewport returned incorrect values! Something is wrong!\n", stderr);
			eglDestroyContext(display, context);
			eglDestroySurface(display, surface);
			eglTerminate(display);
			gbmClean(device, crtc, &connectorId);
			assert(false);
return EXIT_FAILURE;
		}
	}

	// pucgenie: Let's experiment ;)
	if (false) {
		// Clear whole screen (front buffer)
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	SPLASHSCREEN_TRACE(fputs("Before create program\n", stderr));
	// Create a shader program
	// NO ERRRO CHECKING IS DONE! (for the purpose of this example)
	// Read an OpenGL tutorial to properly implement shader creation
	GLuint program = glCreateProgram();
	glUseProgram(program);
	
	const char *fragmentShaderCode =
		STRINGIFY(uniform vec4 color; void main() { gl_FragColor = vec4(color); });
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &fragmentShaderCode, NULL);
	// pucgenie: TODO: statically precompile shader
	glCompileShader(frag);
	glAttachShader(program, frag);

	const char *vertexShaderCode =
		STRINGIFY(attribute vec3 pos; void main() { gl_Position = vec4(pos, 1.0); });
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &vertexShaderCode, NULL);
	glCompileShader(vert);
	glAttachShader(program, vert);
	
	SPLASHSCREEN_TRACE(fputs("Before link program\n", stderr));
	glLinkProgram(program);
	SPLASHSCREEN_TRACE(fputs("Before use program\n", stderr));
	glUseProgram(program);

	SPLASHSCREEN_TRACE(fputs("Before vbo\n", stderr));
	GLuint vbo;
	// Create Vertex Buffer Object
	// Again, NO ERRRO CHECKING IS DONE! (for the purpose of this example)
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// The following array holds vec3 data of
	// three vertex positions
	const GLfloat vertices[] = {
		-1.0f, -1.0f, +0.0f,
		+1.0f, -1.0f, +0.0f,
		+0.0f, +1.0f, +0.0f,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

	// Get vertex attribute and uniform locations
	posLoc = glGetAttribLocation(program, "pos");
	colorLoc = glGetUniformLocation(program, "color");

	// Set the desired color of the triangle to pink
	// 100% red, 0% green, 50% blue, 100% alpha
	glUniform4f(colorLoc, 1.0, 0.0f, 0.5, 1.0);

	// Set our vertex data
	glEnableVertexAttribArray(posLoc);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
						  (void *)0);

	// Render a triangle consisting of 3 vertices to the back buffer
	glDrawArrays(GL_TRIANGLES, 0, 3);

	//SPLASHSCREEN_TRACE(fputs("Before swap buffers\n", stderr));
	// in order to display what you drew you need to swap the back and front buffers.
	gbmSwapBuffers(device, display, surface, &mode, &connectorId, crtc);

	// TODO: draw more, animate, etc.

	SPLASHSCREEN_TRACE(fputs("Before destroy/cleanup\n", stderr));
	// no need to draw again - Cleanup
	eglDestroyContext(display, context);
	eglDestroySurface(display, surface);
	eglTerminate(display);


	sleep(5); // pause for a moment so that you can see it worked before returning to command line

	// really clears the screen
	gbmClean(device, crtc, &connectorId);
	close(device);
return EXIT_SUCCESS;
}


// Get the EGL error back as a string. Useful for debugging.
// pucgenie: Implementation detail: These pointers are valid after return because the strings are stored somewhere in .text or .rodata
static const char *eglGetErrorStr() {
	switch (eglGetError()) {
	case EGL_SUCCESS:
return "The last function succeeded without error.";
	case EGL_NOT_INITIALIZED:
return "EGL is not initialized, or could not be initialized, for the "
			   "specified EGL display connection.";
	case EGL_BAD_ACCESS:
return "EGL cannot access a requested resource (for example a context "
			   "is bound in another thread).";
	case EGL_BAD_ALLOC:
return "EGL failed to allocate resources for the requested operation.";
	case EGL_BAD_ATTRIBUTE:
return "An unrecognized attribute or attribute value was passed in the "
			   "attribute list.";
	case EGL_BAD_CONTEXT:
return "An EGLContext argument does not name a valid EGL rendering "
			   "context.";
	case EGL_BAD_CONFIG:
return "An EGLConfig argument does not name a valid EGL frame buffer "
			   "configuration.";
	case EGL_BAD_CURRENT_SURFACE:
return "The current surface of the calling thread is a window, pixel "
			   "buffer or pixmap that is no longer valid.";
	case EGL_BAD_DISPLAY:
return "An EGLDisplay argument does not name a valid EGL display "
			   "connection.";
	case EGL_BAD_SURFACE:
return "An EGLSurface argument does not name a valid surface (window, "
			   "pixel buffer or pixmap) configured for GL rendering.";
	case EGL_BAD_MATCH:
return "Arguments are inconsistent (for example, a valid context "
			   "requires buffers not supplied by a valid surface).";
	case EGL_BAD_PARAMETER:
return "One or more argument values are invalid.";
	case EGL_BAD_NATIVE_PIXMAP:
return "A NativePixmapType argument does not refer to a valid native "
			   "pixmap.";
	case EGL_BAD_NATIVE_WINDOW:
return "A NativeWindowType argument does not refer to a valid native "
			   "window.";
	case EGL_CONTEXT_LOST:
return "A power management event has occurred. The application must "
			   "destroy all contexts and reinitialise OpenGL ES state and "
			   "objects to continue rendering.";
	default:
		break;
	}
return "Unknown error!";
}
