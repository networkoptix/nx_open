#include "gl_widget_factory.h"
#ifdef Q_OS_WINDOWS
#include <QtCore/qt_windows.h>
#endif
#ifdef Q_OS_LINUX
#include <GL/glx.h>
#include <QtX11Extras/QX11Info>
#endif
#include <QtGui/QOpenGLContext>

void QnGlWidgetFactory::enableVSync(QGLWidget *widget) {
    widget->makeCurrent();

#if defined(Q_OS_WIN32)
    WglSwapIntervalExt wglSwapIntervalExt = ((WglSwapIntervalExt)wglGetProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalExt)
        wglSwapIntervalExt(1);
#elif defined(Q_OS_MAC)
    // TODO: #dklychkov Implement vsync for mac
#elif defined(Q_OS_LINUX)
    typedef int (*fn_glXSwapIntervalSGI)(int);
    QByteArray extensions(glXQueryExtensionsString(QX11Info::display(), QX11Info::appScreen()));
    if (extensions.contains("GLX_SGI_swap_control")) {
        fn_glXSwapIntervalSGI glXSwapIntervalSGI = reinterpret_cast<fn_glXSwapIntervalSGI>(widget->context()->getProcAddress(QString::fromLatin1("glXSwapIntervalSGI")));
        if (glXSwapIntervalSGI)
            glXSwapIntervalSGI(1);
    }
    // TODO: #dklychkov Implement vsync for linux
#endif
}
