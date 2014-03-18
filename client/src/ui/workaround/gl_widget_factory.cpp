#include "gl_widget_factory.h"

#ifdef Q_OS_LINUX
#   include <GL/glx.h>
#   include <QtX11Extras/QX11Info>
#endif

#include <QtGui/QOpenGLContext>

QGLFormat QnGlWidgetFactory::createDefaultFormat() {
    QGLFormat result;

    if(qnSettings->lightMode() & Qn::LightModeNoMultisampling) {
        result.setSampleBuffers(false);
    } else {
        result.setSampleBuffers(true);
        result.setSamples(2);
    }

    result.setDoubleBuffer(qnSettings->isGlDoubleBuffer());
    
    /* Unfortunately, in Qt5 this function is broken :( */
    // format.setSwapInterval(1);

    return result;
}

void QnGlWidgetFactory::enableVSync(QGLWidget *widget) {
    widget->makeCurrent();

#if defined(Q_OS_WIN32)
    typedef BOOL (WINAPI *fn_wglSwapIntervalExt)(int);
    fn_wglSwapIntervalExt wglSwapIntervalExt = reinterpret_cast<fn_wglSwapIntervalExt>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalExt)
        wglSwapIntervalExt(1);
#elif defined(Q_OS_MAC)
    // TODO: #dklychkov Implement vsync for mac
#elif defined(Q_OS_LINUX)
    typedef int (*fn_glXSwapIntervalSGI)(int);
    QByteArray extensions(glXQueryExtensionsString(QX11Info::display(), QX11Info::appScreen()));
    if (extensions.contains("GLX_SGI_swap_control")) {
        fn_glXSwapIntervalSGI glXSwapIntervalSGI = reinterpret_cast<fn_glXSwapIntervalSGI>(widget->context()->getProcAddress(lit("glXSwapIntervalSGI")));
        if (glXSwapIntervalSGI)
            glXSwapIntervalSGI(1);
    }
    // TODO: #dklychkov VSync should but does not work this way => find other way
#endif
}
