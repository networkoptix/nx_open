#include "rendering_widget.h"

#include <camera/resource_display.h>
#include <ui/graphics/items/resource_widget_renderer.h>

QnRenderingWidget::QnRenderingWidget(const QGLFormat &format, QWidget *parent, const QGLWidget *shareWidget, Qt::WindowFlags f):
    QGLWidget(format, parent, shareWidget, f),
    m_display(NULL),
    m_renderer(NULL)
{
}

QnRenderingWidget::~QnRenderingWidget() {
    return;
}

QnMediaResourcePtr QnRenderingWidget::resource() const {
    if(!m_display)
        return QnMediaResourcePtr();

    return m_display->mediaResource();
}

void QnRenderingWidget::setResource(const QnMediaResourcePtr &resource) {
    if(resource == this->resource())
        return;

    if(m_display) {
        delete m_display;
        m_display = NULL;
        m_renderer = NULL;
    }
    
    if(resource) {
        m_display = new QnResourceDisplay(resource, this);
        m_renderer = new QnResourceWidgetRenderer(1, NULL, context());
        m_display->addRenderer(m_renderer); /* Ownership of the renderer is transferred to the display. */
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRenderingWidget::resizeEvent(QResizeEvent *event) {

}

void QnRenderingWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    
    painter.beginNativePainting();
    if (m_display)
        m_display->paint(0, m_videoRect, 1.0);
    painter.endNativePainting();
    
    if (m_firstTime)
    {
        if (m_camDisplay)
            m_camDisplay->start();
        m_firstTime = false;
    }

}
