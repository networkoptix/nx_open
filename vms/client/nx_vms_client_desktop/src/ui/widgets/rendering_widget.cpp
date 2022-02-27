// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rendering_widget.h"

#include <QtCore/QTimer>
#include <QtGui/QScreen>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <core/resource/media_resource.h>

#include <ui/graphics/items/resource/resource_widget_renderer.h>

#include <nx/streaming/abstract_archive_stream_reader.h>

#include <nx/vms/client/core/utils/geometry.h>

#include "ui/graphics/opengl/gl_shortcuts.h"
#include "opengl_renderer.h"

using nx::vms::client::core::Geometry;

namespace {

constexpr qreal kDefaultAspectRatio = 16.0 / 9.0;

QSize rendererSourceSize(const QnResourceWidgetRenderer* renderer)
{
    return renderer ? renderer->sourceSize() : QSize();
}

} // namespace


QnRenderingWidget::QnRenderingWidget(QWidget* parent, Qt::WindowFlags flags):
    QOpenGLWidget(parent, flags)
{
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() { update(); });
    timer->start(1000 / 60);
}

QnRenderingWidget::~QnRenderingWidget()
{
    if (!m_display)
        return;

    m_renderer->disconnect(this);
    m_display->removeRenderer(m_renderer);
    m_renderer->destroyAsync();
    m_display->beforeDestroy();
    m_display.clear();
}

QnMediaResourcePtr QnRenderingWidget::resource() const
{
    return m_resource;
}

void QnRenderingWidget::setResource(const QnMediaResourcePtr& resource)
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
    m_renderer = nullptr;

    m_display->beforeDestroy();
    m_display.clear();

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
    QSize result = rendererSourceSize(m_renderer);
    if (result.isEmpty())
        return QSize();

    return result.expandedTo(minimumSizeHint());
}

QSize QnRenderingWidget::minimumSizeHint() const
{
    QSize result = rendererSourceSize(m_renderer);
    if (result.isEmpty())
        return QSize();

    if (m_effectiveWidth > 0)
    {
        return Geometry::bounded(result,
            QSizeF(m_effectiveWidth, m_effectiveWidth),
            Qt::KeepAspectRatio).toSize();
    }

    return result;
}

bool QnRenderingWidget::hasHeightForWidth() const
{
    return true;
}

int QnRenderingWidget::heightForWidth(int width) const
{
    QSize size = rendererSourceSize(m_renderer);
    auto aspectRatio = size.isEmpty()
        ? kDefaultAspectRatio
        : Geometry::aspectRatio(size);

    return qRound(width / aspectRatio);
}

void QnRenderingWidget::updateChannelScreenSize()
{
    if (!m_renderer)
    {
        m_channelScreenSize = QSize();
    }
    else
    {
        QSize channelScreenSize = Geometry::cwiseDiv(size(), m_display->videoLayout()->size()).toSize();
        if (channelScreenSize != m_channelScreenSize)
        {
            m_channelScreenSize = channelScreenSize;
            m_renderer->setChannelScreenSize(m_channelScreenSize);
        }
    }
}

void QnRenderingWidget::invalidateDisplay()
{
    if (!m_display)
        return;

    m_renderer->disconnect(this);
    m_display.reset();
    m_renderer = nullptr; /*< Owned by display */
}

void QnRenderingWidget::ensureDisplay()
{
    if (m_display || !m_resource)
        return;

    m_display.reset(new QnResourceDisplay(m_resource->toResourcePtr()));
    m_renderer = new QnResourceWidgetRenderer(nullptr, this);
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
    QOpenGLWidget::initializeGL();
    invalidateDisplay(); /* OpenGL context may have changed. */
    const auto functions = context()->functions();
    functions->glClearColor(0, 0, 0, 0);
    functions->glDisable(GL_DEPTH_TEST);
    functions->glDisable(GL_CULL_FACE);
}

void QnRenderingWidget::resizeGL(int width, int height)
{
    context()->functions()->glViewport(0, 0, width, height);

    auto renderer = QnOpenGLRendererManager::instance(this);

    QMatrix4x4 matrix;
    matrix.translate(-1.0, 1.0, 0.0);

    matrix.scale(2.0 / width, - 2.0 / height, 1.0);
    renderer->setProjectionMatrix(matrix);
}

void QnRenderingWidget::paintGL()
{
    context()->functions()->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
            Geometry::expanded(Geometry::aspectRatio(sourceSize), rect(), Qt::KeepAspectRatio, Qt::AlignCenter),
            1.0);
    }
}

