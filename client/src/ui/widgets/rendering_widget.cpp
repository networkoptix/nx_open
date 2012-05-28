#include "rendering_widget.h"

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/graphics/items/resource_widget_renderer.h>
#include <ui/common/scene_utility.h>

#include <plugins/resources/archive/abstract_archive_stream_reader.h>



#include "ui/graphics/opengl/gl_shortcuts.h"

namespace {
    QGLFormat qn_renderingWidgetFormat() {
        QGLFormat result;
        result.setOption(QGL::SampleBuffers); /* Multisampling. */
        result.setSwapInterval(1); /* Turn vsync on. */
        return result;
    }

} // anonymous namespace


QnRenderingWidget::QnRenderingWidget(QWidget *parent, Qt::WindowFlags f):
    QGLWidget(qn_renderingWidgetFormat(), parent, NULL, f),
    m_display(NULL),
    m_renderer(NULL)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000 / 60);
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
        m_channelScreenSize = QSize();
        m_display = new QnResourceDisplay(resource, this);
        m_renderer = new QnResourceWidgetRenderer(1, NULL, context());
        m_display->addRenderer(m_renderer); /* Ownership of the renderer is transferred to the display. */
        m_display->start();

        if(m_display->archiveReader())
            m_display->archiveReader()->setCycleMode(false);
        m_display->camDisplay()->setMTDecoding(true);
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRenderingWidget::initializeGL() {
    glClearColor(0, 0, 0, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void QnRenderingWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glTranslated(-1.0, 1.0, 0.0);
    glScaled(2.0 / width, -2.0 / height, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

void QnRenderingWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_renderer) {
        /* Update screen size of a single channel. */
        QSize channelScreenSize = QSizeF(width() / m_display->videoLayout()->width(), height() / m_display->videoLayout()->height()).toSize();
        if(channelScreenSize != m_channelScreenSize) {
            m_channelScreenSize = channelScreenSize;
            m_renderer->setChannelScreenSize(m_channelScreenSize);
        }

        /* Paint frame. */
        QSize sourceSize = m_renderer->sourceSize();
        if(sourceSize.isEmpty())
            sourceSize = size();

        glLoadIdentity();
        m_renderer->paint(
            0,
            SceneUtility::expanded(SceneUtility::aspectRatio(sourceSize), rect(), Qt::KeepAspectRatio, Qt::AlignCenter),
            1.0
        );
    }
}

