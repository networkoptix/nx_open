#ifndef QN_RENDERING_WIDGET_H
#define QN_RENDERING_WIDGET_H

#include <QtOpenGL/QGLWidget>

#include <core/resource/resource_fwd.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;

/**
 * Widget for displaying video from the given resource without constructing 
 * the heavy graphics scene machinery.
 */
class QnRenderingWidget: public QGLWidget {
    Q_OBJECT;
public:
    QnRenderingWidget(QWidget *parent = 0, Qt::WindowFlags f = 0);

    virtual ~QnRenderingWidget();

    QnMediaResourcePtr resource() const;
    void setResource(const QnMediaResourcePtr &resource);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int width, int height) override;

private:
    QnResourceDisplay *m_display;
    QnResourceWidgetRenderer *m_renderer;
    QSize m_channelScreenSize;
};


#endif // QN_RENDERING_WIDGET_H
