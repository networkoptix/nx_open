#include "rendering_widget.h"

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <core/resource/media_resource.h>

#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/common/geometry.h>

#include <nx/streaming/abstract_archive_stream_reader.h>

#include "ui/graphics/opengl/gl_shortcuts.h"
#include "opengl_renderer.h"

QnRenderingWidget::QnRenderingWidget(const QGLFormat &format, QWidget *parent, QGLWidget *shareWidget, Qt::WindowFlags f):
    QnGLWidget(format, parent, shareWidget, f),
    m_display(NULL),
    m_renderer(NULL)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(1000 / 60);
}

QnRenderingWidget::~QnRenderingWidget()
{
    if (m_display)
    {
        m_renderer->disconnect(this);
        m_display->removeRenderer(m_renderer);
        m_renderer->destroyAsync();
        m_display->beforeDestroy();
        delete m_display;
    }
}

QnMediaResourcePtr QnRenderingWidget::resource() const
{
    return m_resource;
}

void QnRenderingWidget::setResource(const QnMediaResourcePtr &resource)
{
    if (resource == m_resource)
        return;

    invalidateDisplay();

    m_resource = resource;
}

void QnRenderingWidget::stopPlayback()
{
    if (!m_display)
        return;

    m_display->removeRenderer(m_renderer);
    m_renderer->destroyAsync();
    m_renderer = NULL;

    m_display->beforeDestroy();
    delete m_display;
    m_display = NULL;

    m_channelScreenSize = QSize();
}

void QnRenderingWidget::setEffectiveWidth(int value)
{
    if (m_effectiveWidth == value)
        return;

    m_effectiveWidth = value;
    updateGeometry();
}

int QnRenderingWidget::effectiveWidth() const
{
    return m_effectiveWidth;
}

QSize QnRenderingWidget::sizeHint() const
{
    if (!m_renderer)
        return QSize();

    return m_renderer->sourceSize();
}

QSize QnRenderingWidget::minimumSizeHint() const
{
    if (!m_renderer)
        return QSize();

    QSize result = m_renderer->sourceSize();
    if (!result.isValid())
        return result;

    if (m_effectiveWidth > 0)
    {
        return QnGeometry::bounded(result, QSizeF(m_effectiveWidth, m_effectiveWidth),
            Qt::KeepAspectRatio).toSize();
    }

    return result;
}

void QnRenderingWidget::updateChannelScreenSize()
{
    if (!m_renderer)
    {
        m_channelScreenSize = QSize();
    }
    else
    {
        QSize channelScreenSize = QnGeometry::cwiseDiv(size(), m_display->videoLayout()->size()).toSize();
        if (channelScreenSize != m_channelScreenSize)
        {
            m_channelScreenSize = channelScreenSize;
            m_renderer->setChannelScreenSize(m_channelScreenSize);
        }
    }
}

void QnRenderingWidget::invalidateDisplay()
{
    if (m_display)
    {
        m_renderer->disconnect(this);
        delete m_display;
        m_display = NULL;
        m_renderer = NULL; /*< Owned by display */
    }
}

void QnRenderingWidget::ensureDisplay()
{
    if (m_display || !m_resource)
        return;

    m_display = new QnResourceDisplay(m_resource->toResourcePtr(), this);
    m_renderer = new QnResourceWidgetRenderer(NULL, context());
    connect(m_renderer, &QnResourceWidgetRenderer::sourceSizeChanged, this,
        &QWidget::updateGeometry);

    updateChannelScreenSize();

    m_display->addRenderer(m_renderer);
    m_display->start();

    if (m_display->archiveReader())
        m_display->archiveReader()->setCycleMode(false);
    m_display->camDisplay()->setMTDecoding(true);

    updateGeometry();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRenderingWidget::initializeGL()
{
    QnGLWidget::initializeGL();
    invalidateDisplay(); /* OpenGL context may have changed. */

    glClearColor(0, 0, 0, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void QnRenderingWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);

    auto renderer = QnOpenGLRendererManager::instance(context());

    QMatrix4x4 matrix;
    matrix.translate(-1.0, 1.0, 0.0);

    const auto aspect = QGLContext::currentContext()->device()->devicePixelRatio();
    const auto scaleBase = 2.0 * aspect;
    matrix.scale(scaleBase / width, -scaleBase / height, 1.0);
    renderer->setProjectionMatrix(matrix);
}

void QnRenderingWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ensureDisplay();

    if (m_renderer)
    {
        updateChannelScreenSize();

        QSize sourceSize = m_renderer->sourceSize();
        if (sourceSize.isEmpty())
            sourceSize = size();

        m_renderer->paint(
            0,
            QRectF(0.0, 0.0, 1.0, 1.0),
            QnGeometry::expanded(QnGeometry::aspectRatio(sourceSize), rect(), Qt::KeepAspectRatio, Qt::AlignCenter),
            1.0
        );
    }
}

