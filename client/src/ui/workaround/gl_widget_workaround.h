#ifndef QN_GL_WIDGET_WORKAROUND_H
#define QN_GL_WIDGET_WORKAROUND_H

#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLContext>

class QnGLWidget : public QGLWidget {
public:
    using QGLWidget::QGLWidget;

    ~QnGLWidget() {
#ifdef Q_OS_MACX
        // TODO: #dklychkov workaround for QTBUG-36820 that must be fixed soon
        if (context())
            context()->doneCurrent();
#endif
    }
};

#endif // QN_GL_WIDGET_WORKAROUND_H
