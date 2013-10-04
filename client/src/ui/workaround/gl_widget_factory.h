#ifndef QN_GL_WIDGET_FACTORY_H
#define QN_GL_WIDGET_FACTORY_H

#include <QGLFormat>
#include <QGLWidget>
#include "client/client_settings.h"

class QnGlWidgetFactory {
public:
    template<class Widget>
    static Widget *create(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        QGLFormat format;
        format.setOption(QGL::SampleBuffers); /* Multisampling. */
        return create<Widget>(format, parent, windowFlags);
    }

    template<class Widget>
    static Widget *create(const QGLFormat &format, QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        QGLFormat localFormat = format;
#ifdef Q_OS_LINUX
        /* Linux NVidia drivers contain bug that leads to application hanging if VSync is on.
         * VSync will be re-enabled later if drivers are not NVidia's. */
        localFormat.setSwapInterval(0); /* Turn vsync off. */
#else
        localFormat.setSwapInterval(1); /* Turn vsync on. */
#endif
        localFormat.setDoubleBuffer(qnSettings->isGlDoubleBuffer());

        Widget *result = new Widget(localFormat, parent, /* shareWidget = */ NULL, windowFlags);

#ifdef Q_OS_LINUX
        result->makeCurrent();
        QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        if (!vendor.toLower().contains("nvidia")) {
            QGLFormat localFormat = result->format();
            localFormat.setSwapInterval(1); /* Turn vsync on. */
            result->setFormat(localFormat);
        }
#endif

        return result;
    }
};

#endif // QN_GL_WIDGET_FACTORY_H
