#ifndef QN_RENDERING_WIDGET_H
#define QN_RENDERING_WIDGET_H

#include <QtOpenGL/QGLWidget>

#include <core/resource/resource_fwd.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;

class QnRenderingWidget: public QGLWidget {
    Q_OBJECT;
public:
    QnRenderingWidget(const QGLFormat &format, QWidget *parent = 0, const QGLWidget *shareWidget = 0, Qt::WindowFlags f = 0);

    virtual ~QnRenderingWidget();

    QnMediaResourcePtr resource() const;
    void setResource(const QnMediaResourcePtr &resource);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;

private:
    QnResourceDisplay *m_display;
    QnResourceWidgetRenderer *m_renderer;
};


#endif // QN_RENDERING_WIDGET_H
