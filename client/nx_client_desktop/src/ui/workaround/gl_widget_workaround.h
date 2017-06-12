#ifndef QN_GL_WIDGET_WORKAROUND_H
#define QN_GL_WIDGET_WORKAROUND_H

#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLContext>

class QnGLWidget : public QGLWidget {
public:
    explicit QnGLWidget(QWidget* parent=0,
                        const QGLWidget* shareWidget = 0, Qt::WindowFlags f=0)
        : QGLWidget(parent, shareWidget, f)
    {}
    explicit QnGLWidget(QGLContext *context, QWidget* parent=0,
                        const QGLWidget* shareWidget = 0, Qt::WindowFlags f=0)
        : QGLWidget(context, parent, shareWidget, f)
    {}
    explicit QnGLWidget(const QGLFormat& format, QWidget* parent=0,
                        const QGLWidget* shareWidget = 0, Qt::WindowFlags f=0)
        : QGLWidget(format, parent, shareWidget, f)
    {}

    ~QnGLWidget() {
#ifdef Q_OS_MACX
        // TODO: #dklychkov workaround for QTBUG-36820 that must be fixed soon
        if (context())
            context()->doneCurrent();
#endif
    }

private:
    Q_DISABLE_COPY(QnGLWidget)
};

#endif // QN_GL_WIDGET_WORKAROUND_H
