#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <iostream>
#include <string>
#include <algorithm>

using namespace std;

void displayStrings(const char* s, const char* label)
{
    string strings = s ? s : "";
    cout << label << ": " << (s ? "" : "none") << endl;

    std::replace(strings.begin(), strings.end(), ' ', '\n');
    cout << strings << endl;
}

void displayStrings(EGLDisplay display, EGLint type, const char* label)
{
    displayStrings(eglQueryString(display, type), label);
}

int main()
{
    EGLint major, minor;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLBoolean r = eglInitialize(display, &major, &minor);

    if (!r)
    {
        cout << "Cannot initialize EGL!" << endl;
        return -1;
    }

    cout << "EGL version: " << major << "." << minor << "\n" << endl;

    displayStrings(EGL_NO_DISPLAY, EGL_EXTENSIONS, "Client extensions");
    displayStrings(display, EGL_EXTENSIONS, "Display extensions");

    int ver = major * 100 + minor;
    if (ver > 102)
        displayStrings(display, EGL_CLIENT_APIS, "APIs");

    cout << endl;

    EGLint attributes[] =
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig configs[1];
    EGLint num_configs;
    r = eglChooseConfig(display, attributes, configs, 1, &num_configs);

    if (!num_configs)
    {
        cout << "No suitable configurations to create OpenGL ES context!" << endl;
        eglTerminate(display);
        return -2;
    }

    EGLint minInterval(-1), maxInterval(-1);
    eglGetConfigAttrib(display, configs[0], EGL_MIN_SWAP_INTERVAL, &minInterval);
    eglGetConfigAttrib(display, configs[0], EGL_MAX_SWAP_INTERVAL, &maxInterval);

    cout << "Min swap interval: " << minInterval << endl;
    cout << "Max swap interval: " << maxInterval << endl;

    EGLContext context = EGL_NO_CONTEXT;
    for (int ver = 10; ver > 0; --ver)
    {
        EGLint context_attributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, ver,
            EGL_NONE
        };

        context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, context_attributes);
        if (context)
            break;
    }

    if (!context)
    {
        cout << "Failed to create OpenGL ES context!" << endl;
        eglTerminate(display);
        return -3;
    }

    EGLint pbuffer_attributes[] = {
        EGL_HEIGHT, 16,
        EGL_WIDTH, 16,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, configs[0], pbuffer_attributes);
    if (!surface)
    {
        cout << "Failed to create renderable surface!" << endl;
        eglDestroyContext(display, context);
        eglTerminate(display);
        return -4;
    }

    eglMakeCurrent(display, surface, surface, context);
    cout << flush;

    cout << "OpenGL ES version: " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << "\n" << endl;
    displayStrings(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), "OpenGL ES extensions");

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return 0;
}
