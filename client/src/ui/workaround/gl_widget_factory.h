#ifndef QN_GL_WIDGET_FACTORY_H
#define QN_GL_WIDGET_FACTORY_H

#include <QtOpenGL/QGLFormat>
#include <QtOpenGL/QGLWidget>
#include "client/client_settings.h"

class QnGlWidgetFactory {
public:
    template<class Widget>
    static Widget *create(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        return create<Widget>(createDefaultFormat(), parent, windowFlags);
    }

    template<class Widget>
    static Widget *create(const QGLFormat &format, QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) {
        /* We cannot create QGLContext without painting device, thus we use context created by GL widget. */
        Widget *widget = new Widget(format, parent, /* shareWidget = */ NULL, windowFlags);

        if (qnSettings->isVSyncEnabled()) {
            /* In Qt5 QGLContext uses QOpenGLContext internally. But QOpenGLContext hasn't support for
               swap interval setting (up to and including Qt5.2). So we have to implement vsync manually. */
            enableVSync(widget);
        }

        return widget;
    }

private:
    static QGLFormat createDefaultFormat();
    static void enableVSync(QGLWidget *widget);
};

#endif // QN_GL_WIDGET_FACTORY_H
