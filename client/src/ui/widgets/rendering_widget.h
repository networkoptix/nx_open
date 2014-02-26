#ifndef QN_RENDERING_WIDGET_H
#define QN_RENDERING_WIDGET_H

#include <QtOpenGL/QGLWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workaround/gl_widget_workaround.h>

class QnResourceDisplay;
class QnResourceWidgetRenderer;

/**
 * Widget for displaying video from the given resource without constructing 
 * the heavy graphics scene machinery.
 */
class QnRenderingWidget: public QnGLWidget {
    Q_OBJECT;
public:
    QnRenderingWidget(const QGLFormat &format, QWidget *parent = 0, QGLWidget *shareWidget = NULL, Qt::WindowFlags f = 0);
    virtual ~QnRenderingWidget();

    QnMediaResourcePtr resource() const;
    void setResource(const QnMediaResourcePtr &resource);

    void stopPlayback();

protected:
    void updateChannelScreenSize();

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int width, int height) override;

    void invalidateDisplay();
    void ensureDisplay();

private:
    QnMediaResourcePtr m_resource;
    QnResourceDisplay *m_display;
    QnResourceWidgetRenderer *m_renderer;
    QSize m_channelScreenSize;
};


#endif // QN_RENDERING_WIDGET_H
