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
        return new Widget(createGLContext(format), parent, /* shareWidget = */ NULL, windowFlags);
    }

private:
    static QGLContext *createGLContext(const QGLFormat &format) {
        QGLFormat localFormat = format;
#ifdef Q_OS_LINUX
        static enum Vendor { Unknown, Nvidia, Other } vendorStatus = Unknown;

        /* Linux NVidia drivers contain bug that leads to application hanging if VSync is on.
         * VSync will be re-enabled later if drivers are not NVidia's. */
        localFormat.setSwapInterval(vendorStatus == Other ? 1 : 0); /* Turn vsync off for nVidia and unknown driver. */
#else
        localFormat.setSwapInterval(1); /* Turn vsync on. */
#endif
        localFormat.setDoubleBuffer(qnSettings->isGlDoubleBuffer());

        QGLContext *glContext = new QGLContext(localFormat);
        glContext->create();

#ifdef Q_OS_LINUX
        if (vendorStatus == Unknown) {
            glContext->makeCurrent();
            QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
            if (!vendor.toLower().contains("nvidia")) {
                vendorStatus = Other;

                localFormat = glContext->format();
                localFormat.setSwapInterval(1); /* Turn vsync on. */

                delete glContext;

                glContext = new QGLContext(localFormat);
                glContext->create();
            } else {
                vendorStatus = Nvidia;
            }
        }
#endif

        return glContext;
    }
};

#endif // QN_GL_WIDGET_FACTORY_H
