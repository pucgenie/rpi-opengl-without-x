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
#include <stdio.h>

#include <assert.h>

// The following code related to DRM/GBM was adapted from the following sources:
// https://github.com/eyelash/tutorials/blob/master/drm-gbm.c
// and
// https://www.raspberrypi.org/forums/viewtopic.php?t=243707#p1499181
//
// I am not the original author of this code, I have only modified it.
// pucgenie: Me too.

// File descriptor index type
typedef int fdI_t;

static struct gbm_device *gbmDevice;
static struct gbm_surface *gbmSurface;

static const char *eglGetErrorStr(); // moved to bottom

static uint32_t previousFb;
static struct gbm_bo *previousBo = NULL;

static void gbmSwapBuffers(const fdI_t device, const EGLDisplay display, const EGLSurface surface, drmModeModeInfo * const mode, uint32_t * const connectorId, const drmModeCrtc * const crtc) {
    eglSwapBuffers(display, surface);
    struct gbm_bo * const bo = gbm_surface_lock_front_buffer(gbmSurface);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t pitch = gbm_bo_get_stride(bo);
    uint32_t fb;
    drmModeAddFB(device, mode->hdisplay, mode->vdisplay, 24, 32, pitch, handle, &fb);
    drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, connectorId, 1, mode);

    if (previousBo) {
        drmModeRmFB(device, previousFb);
        // pucgenie: TODO: Reuse buffer? We can assume that the size doesn't change.
        gbm_surface_release_buffer(gbmSurface, previousBo);
    }
    previousBo = bo;
    previousFb = fb;
}

static void gbmClean(const fdI_t device, drmModeCrtc * const crtc, uint32_t * const connectorId) {
    // set the previous crtc
    drmModeSetCrtc(device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, connectorId, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);

    if (previousBo) {
        drmModeRmFB(device, previousFb);
        gbm_surface_release_buffer(gbmSurface, previousBo);
    }

    gbm_surface_destroy(gbmSurface);
    gbm_device_destroy(gbmDevice);
}

// The following array holds vec3 data of
// three vertex positions
static const GLfloat vertices[] = {
    -1.0f, -1.0f, +0.0f,
    +1.0f, -1.0f, +0.0f,
    +0.0f, +1.0f, +0.0f,
};

// The following are GLSL shaders for rendering a triangle on the screen
#define STRINGIFY(x) #x
static const char *vertexShaderCode = STRINGIFY(
    attribute vec3 pos; void main() { gl_Position = vec4(pos, 1.0); });

static const char *fragmentShaderCode =
    STRINGIFY(uniform vec4 color; void main() { gl_FragColor = vec4(color); });

int main() {
    const char* TRY_CARDS[] = {
        "/dev/dri/card1",
        "/dev/dri/card0",
    };

    static fdI_t device;
    static drmModeCrtc *crtc;
    static uint32_t connectorId;
    static drmModeModeInfo mode;

    // we have to try card0 and card1 to see which is valid. fopen will work on both, so...
    device = open(TRY_CARDS[0], O_RDWR | O_CLOEXEC);
    {
        drmModeRes *resources;
        if ((resources = drmModeGetResources(device)) == NULL) {
            // if we have the right device we can get it's resources
            printf("/dev/dri/card1 does not have DRM resources, using card0, ");
            device = open(TRY_CARDS[1], O_RDWR | O_CLOEXEC); // if not, try the other one: (1)
            resources = drmModeGetResources(device);
        } else {
          printf("using /dev/dri/card1, ");
        }

        if (resources == NULL) {
            // pucgenie: Why card1 hardcoded in text?
            printf("Unable to get DRM resources on card1\n");
return -1;
        }

        drmModeConnector *connector;
        assert(resources->count_connectors > 0);
        {
            int i = resources->count_connectors;
            for (; i --> 0; drmModeFreeConnector(connector)) {
                connector = drmModeGetConnector(device, resources->connectors[i]);
                if (connector->connection == DRM_MODE_CONNECTED) {
                    break;
                }
            }
            if (i < 0 || i >= resources->count_connectors) {
                fprintf(stderr, "Unable to get connector\n");
                drmModeFreeResources(resources);
return -1;
            }
        }
        connectorId = connector->connector_id;
        // copy: array of resolutions and refresh rates supported by this display
        mode = connector->modes[0];
        printf("resolution: %ix%i\n", mode.hdisplay, mode.vdisplay);

        drmModeEncoder *encoder;
        if (!connector->encoder_id) {
            fprintf(stderr, "Unable to get encoder\n");
            drmModeFreeConnector(connector);
            drmModeFreeResources(resources);
return -1;
        }
        encoder = drmModeGetEncoder(device, connector->encoder_id);
        assert(encoder);

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
            fprintf(stderr, "Failed to get EGL version! Error: %s\n",
                    eglGetErrorStr());
            eglTerminate(display);
            gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
        }

        // Make sure that we can use OpenGL in this EGL app.
        eglBindAPI(EGL_OPENGL_API);

        printf("Initialized EGL version: %d.%d\n", major, minor);
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
                fprintf(stderr, "Failed to get EGL configs! Error: %s\n",
                        eglGetErrorStr());
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
                fprintf(stderr, "Failed to find matching EGL config! Error: %s\n",
                        eglGetErrorStr());
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
                fprintf(stderr, "Failed to create EGL context! Error: %s\n",
                        eglGetErrorStr());
                eglTerminate(display);
                gbmClean(device, crtc, &connectorId);
return EXIT_FAILURE;
            }

            surface = eglCreateWindowSurface(display, configs[configIndex], gbmSurface, NULL);
            if (surface == EGL_NO_SURFACE) {
                fprintf(stderr, "Failed to create EGL surface! Error: %s\n",
                        eglGetErrorStr());
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
        printf("GL Viewport size: %dx%d\n", viewport[2], viewport[3]);

        if (viewport[2] != mode.hdisplay || viewport[3] != mode.vdisplay) {
            fprintf(stderr, "Error! The glViewport returned incorrect values! Something is wrong!\n");
            eglDestroyContext(display, context);
            eglDestroySurface(display, surface);
            eglTerminate(display);
            gbmClean(device, crtc, &connectorId);
            assert(false);
return EXIT_FAILURE;
        }
    }

    // pucgenie: Let's try some experiments ;)
    if (false) {
        // Clear whole screen (front buffer)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    static GLuint program, vert, frag;
    // Create a shader program
    // NO ERRRO CHECKING IS DONE! (for the purpose of this example)
    // Read an OpenGL tutorial to properly implement shader creation
    program = glCreateProgram();
    glUseProgram(program);
    
    vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertexShaderCode, NULL);
    glCompileShader(vert);
    
    frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragmentShaderCode, NULL);
    // pucgenie: TODO: statically precompile shader
    glCompileShader(frag);

    glAttachShader(program, frag);
    glAttachShader(program, vert);
    glLinkProgram(program);
    glUseProgram(program);

    GLuint vbo;
    // Create Vertex Buffer Object
    // Again, NO ERRRO CHECKING IS DONE! (for the purpose of this example)
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

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

    // in order to display what you drew you need to swap the back and front buffers.
    gbmSwapBuffers(device, display, surface, &mode, &connectorId, crtc);

    sleep(5); // pause for a moment so that you can see it worked before returning to command line

    // Cleanup
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    gbmClean(device, crtc, &connectorId);

    close(device);
return EXIT_SUCCESS;
}


// Get the EGL error back as a string. Useful for debugging.
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