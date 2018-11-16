#include "video_item_surface.h"

#include <QtGui/QOpenGLContext>
#include <QtMultimedia/QVideoSurfaceFormat>

#include <QtMultimedia/private/qsgvideonode_p.h>

#include "video_output_backend.h"

QnVideoItemSurface::QnVideoItemSurface(QnVideoOutputBackend* backend, QObject* parent):
    QAbstractVideoSurface(parent),
    m_backend(backend)
{
}

QnVideoItemSurface::~QnVideoItemSurface()
{
}

QList<QVideoFrame::PixelFormat> QnVideoItemSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> formats;

    for (auto factory: m_backend->videoNodeFactories())
        formats.append(factory->supportedPixelFormats(handleType));

    return formats;
}

bool QnVideoItemSurface::start(const QVideoSurfaceFormat& format)
{
    if (!supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return false;

    return QAbstractVideoSurface::start(format);
}

void QnVideoItemSurface::stop()
{
    m_backend->stop();
    QAbstractVideoSurface::stop();
}

bool QnVideoItemSurface::present(const QVideoFrame& frame)
{
    m_backend->present(frame);
    return true;
}

void QnVideoItemSurface::scheduleOpenGLContextUpdate()
{
    // This method is called from render thread
    QMetaObject::invokeMethod(this, "updateOpenGLContext");
}

void QnVideoItemSurface::updateOpenGLContext()
{
    // Set a dynamic property to access the OpenGL context in Qt Quick render thread.
    setProperty("GLContext", QVariant::fromValue<QObject*>(m_backend->glContext()));
}
