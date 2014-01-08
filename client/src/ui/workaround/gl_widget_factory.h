#ifndef QN_GL_WIDGET_FACTORY_H
#define QN_GL_WIDGET_FACTORY_H

#ifdef Q_OS_WINDOWS
#include <QtCore/qt_windows.h>
#endif
#include <QtOpenGL/QGLFormat>
#include <QtOpenGL/QGLWidget>
#include "client/client_settings.h"

class QnGlWidgetFactory {
public:
    template<class Widget>
    static Widget *create(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        QGLFormat format;
        format.setOption(QGL::SampleBuffers); /* Multisampling. */
        format.setDoubleBuffer(qnSettings->isGlDoubleBuffer());
        /* Unfortunately, in Qt5 this function is broken :( */
        // format.setSwapInterval(1);
        return create<Widget>(format, parent, windowFlags);
    }

    template<class Widget>
    static Widget *create(const QGLFormat &format, QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        /* We cannot create QGLContext without painting device, thus we use context created by GL widget. */
        Widget *widget = new Widget(format, parent, /* shareWidget = */ NULL, windowFlags);

        /* In Qt5 QGLContext uses QOpenGLContext internally. But QOpenGLContext hasn't support for
           swap interval (up to Qt5.2). So we have to implement vsync manually. */
        widget->context()->makeCurrent();

    #if defined(Q_OS_WIN32)
        WglSwapIntervalExt wglSwapIntervalExt = ((WglSwapIntervalExt)wglGetProcAddress("wglSwapIntervalEXT"));
        if (wglSwapIntervalExt)
            wglSwapIntervalExt(1);
    #elif defined(Q_OS_MAC)
        // TODO: #dklychkov Implement vsync for mac
    #elif defined(Q_OS_LINUX)
        // TODO: #dklychkov Implement vsync for linux
    #endif


        return widget;
    }
};

#endif // QN_GL_WIDGET_FACTORY_H
