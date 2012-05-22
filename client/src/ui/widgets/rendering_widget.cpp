#include "rendering_widget.h"

#include <camera/resource_display.h>

#include <ui/graphics/items/resource_widget_renderer.h>
#include <ui/common/scene_utility.h>

QnRenderingWidget::QnRenderingWidget(const QGLFormat &format, QWidget *parent, const QGLWidget *shareWidget, Qt::WindowFlags f):
    QGLWidget(format, parent, shareWidget, f),
    m_display(NULL),
    m_renderer(NULL)
{}

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
        m_display->start();
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRenderingWidget::initializeGL() {
    glClearColor(0, 0, 0, 0);
}

void QnRenderingWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (m_renderer) {
        QSize sourceSize = m_renderer->sourceSize();
        if(!sourceSize.isEmpty()) {
            m_renderer->paint(
                0,
                SceneUtility::expanded(SceneUtility::aspectRatio(sourceSize), rect(), Qt::KeepAspectRatio, Qt::AlignCenter),
                1.0
            );
        }
    }
}

