#include "video_output_backend.h"

#include <QtCore/QPointer>
#include <QtCore/QMutex> // TODO: #dklychkov think about QnMutex replacement
#include <QtGui/QOpenGLContext>
#include <QtMultimedia/private/qmediapluginloader_p.h>
#include <QtMultimedia/private/qsgvideonode_p.h>

#include <nx/media/media_player.h>

#include "video_output.h"
#include "video_item_surface.h"
#include "videonode_rgb.h"
#include "videonode_texture.h"
#include "videonode_yuv.h"

Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader,
    videoNodeFactoryLoader,
    (QSGVideoNodeFactoryInterface_iid, QLatin1String("video/videonode"), Qt::CaseInsensitive))

class QnVideoOutputBackendPrivate
{
  public:
    QnVideoOutputBackendPrivate(QnVideoOutput* videoOutput);

  public:
    QnVideoOutput* videoOutput = nullptr;
    QPointer<QVideoRendererControl> rendererControl;
    QList<QSGVideoNodeFactoryInterface*> videoNodeFactories;
    QnVideoItemSurface* surface = nullptr;
    QOpenGLContext* glContext = nullptr;
    QVideoFrame frame;
    bool frameChanged = false;
    QSGVideoNodeFactory_YUV i420Factory;
    QSGVideoNodeFactory_RGB rgbFactory;
    QSGVideoNodeFactory_Texture textureFactory;
    QMutex frameMutex;
    QRectF renderedRect;      // Destination pixel coordinates, clipped
    QRectF sourceTextureRect; // Source texture coordinates
};

QnVideoOutputBackendPrivate::QnVideoOutputBackendPrivate(QnVideoOutput* videoOutput):
    videoOutput(videoOutput)
{
}

QnVideoOutputBackend::QnVideoOutputBackend(QnVideoOutput* videoOutput):
    d_ptr(new QnVideoOutputBackendPrivate(videoOutput))
{
    Q_D(QnVideoOutputBackend);

    d->surface = new QnVideoItemSurface(this);
    QObject::connect(d->surface, &QnVideoItemSurface::surfaceFormatChanged, videoOutput,
        &QnVideoOutput::updateNativeSize, Qt::QueuedConnection);

    for (const auto& key: videoNodeFactoryLoader()->keys())
    {
        auto instance = videoNodeFactoryLoader()->instance(key);
        if (auto plugin = qobject_cast<QSGVideoNodeFactoryInterface*>(instance))
            d->videoNodeFactories.append(plugin);
    }

    d->videoNodeFactories.append(&d->i420Factory);
    d->videoNodeFactories.append(&d->rgbFactory);
    d->videoNodeFactories.append(&d->textureFactory);
}

QnVideoOutputBackend::~QnVideoOutputBackend()
{
    Q_D(QnVideoOutputBackend);
    releasePlayer();
    delete d->surface;
}

void QnVideoOutputBackend::releasePlayer()
{
    Q_D(QnVideoOutputBackend);

    if (auto player = d->videoOutput->player())
    {
        if (player->videoSurface() == d->surface)
            player->setVideoSurface(nullptr);
    }

    d->surface->stop();
}

QSize QnVideoOutputBackend::nativeSize() const
{
    Q_D(const QnVideoOutputBackend);
    return d->surface->surfaceFormat().sizeHint();
}

void QnVideoOutputBackend::updateGeometry()
{
    Q_D(QnVideoOutputBackend);

    const auto surfaceFormat = d->surface->surfaceFormat();
    const auto viewport = QRectF(surfaceFormat.viewport());
    const auto frameSize = QSizeF(surfaceFormat.frameSize());
    const auto normalizedViewport = QRectF(
        viewport.x() / frameSize.width(),
        viewport.y() / frameSize.height(),
        viewport.width() / frameSize.width(),
        viewport.height() / frameSize.height());
    const auto rect = QRectF(0, 0, d->videoOutput->width(), d->videoOutput->height());
    const auto fillMode = d->videoOutput->fillMode();

    if (nativeSize().isEmpty())
    {
        d->renderedRect = rect;
        d->sourceTextureRect = normalizedViewport;
    }
    else if (fillMode == QnVideoOutput::Stretch)
    {
        d->renderedRect = rect;
        d->sourceTextureRect = normalizedViewport;
    }
    else if (fillMode == QnVideoOutput::PreserveAspectFit)
    {
        d->sourceTextureRect = normalizedViewport;
        d->renderedRect = d->videoOutput->contentRect();
    }
    else if (fillMode == QnVideoOutput::PreserveAspectCrop)
    {
        d->renderedRect = rect;
        const qreal contentHeight = d->videoOutput->contentRect().height();
        const qreal contentWidth = d->videoOutput->contentRect().width();

        // Calculate the size of the source rectangle without taking the viewport into account
        const qreal relativeOffsetLeft = -d->videoOutput->contentRect().left() / contentWidth;
        const qreal relativeOffsetTop = -d->videoOutput->contentRect().top() / contentHeight;
        const qreal relativeWidth = rect.width() / contentWidth;
        const qreal relativeHeight = rect.height() / contentHeight;

        // Now take the viewport size into account
        const qreal totalOffsetLeft =
            normalizedViewport.x() + relativeOffsetLeft * normalizedViewport.width();
        const qreal totalOffsetTop =
            normalizedViewport.y() + relativeOffsetTop * normalizedViewport.height();
        const qreal totalWidth = normalizedViewport.width() * relativeWidth;
        const qreal totalHeight = normalizedViewport.height() * relativeHeight;

        d->sourceTextureRect = QRectF(totalOffsetLeft, totalOffsetTop, totalWidth, totalHeight);
    }

    if (surfaceFormat.scanLineDirection() == QVideoSurfaceFormat::BottomToTop)
    {
        qreal top = d->sourceTextureRect.top();
        d->sourceTextureRect.setTop(d->sourceTextureRect.bottom());
        d->sourceTextureRect.setBottom(top);
    }

    if (surfaceFormat.property("mirrored").toBool())
    {
        qreal left = d->sourceTextureRect.left();
        d->sourceTextureRect.setLeft(d->sourceTextureRect.right());
        d->sourceTextureRect.setRight(left);
    }
}

QSGNode* QnVideoOutputBackend::updatePaintNode(
    QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* data)
{
    Q_UNUSED(data);
    Q_D(QnVideoOutputBackend);

    auto videoNode = static_cast<QSGVideoNode*>(oldNode);

    QMutexLocker lock(&d->frameMutex);

    if (!d->glContext)
    {
        d->glContext = QOpenGLContext::currentContext();
        d->surface->scheduleOpenGLContextUpdate();

        // Internal mechanism to call back the surface renderer from the QtQuick render thread
        if (auto object = d->surface->property("_q_GLThreadCallback").value<QObject*>())
        {
            QEvent event(QEvent::User);
            object->event(&event);
        }
    }

    bool isFrameModified = false;
    if (d->frameChanged)
    {
        if (videoNode &&
            (videoNode->pixelFormat() != d->frame.pixelFormat()
                || videoNode->handleType() != d->frame.handleType()))
        {
            delete videoNode;
            videoNode = nullptr;
        }

        if (!d->frame.isValid())
        {
            d->frameChanged = false;
            return nullptr;
        }

        if (!videoNode)
        {
            for (auto factory: d->videoNodeFactories)
            {
                videoNode = factory->createNode(QVideoSurfaceFormat(
                    d->frame.size(), d->frame.pixelFormat(), d->frame.handleType()));

                if (videoNode)
                    break;
            }
        }
    }

    if (!videoNode)
    {
        d->frameChanged = false;
        d->frame = QVideoFrame();
        return nullptr;
    }

    videoNode->setTexturedRectGeometry(d->renderedRect, d->sourceTextureRect, 0);
    if (d->frameChanged)
    {
        QSGVideoNode::FrameFlags flags = 0;
        if (isFrameModified)
            flags |= QSGVideoNode::FrameFiltered;
        videoNode->setCurrentFrame(d->frame, flags);

        // don't keep the frame for more than really necessary
        d->frameChanged = false;
        d->frame = QVideoFrame();
    }
    return videoNode;
}

QAbstractVideoSurface* QnVideoOutputBackend::videoSurface() const
{
    Q_D(const QnVideoOutputBackend);
    return d->surface;
}

QRectF QnVideoOutputBackend::adjustedViewport() const
{
    Q_D(const QnVideoOutputBackend);

    const auto viewportRect = d->surface->surfaceFormat().viewport();
    const auto pixelAspectRatio = d->surface->surfaceFormat().pixelAspectRatio();

    if (pixelAspectRatio.height() > 0)
    {
        const auto ratio = pixelAspectRatio.width() / pixelAspectRatio.height();
        auto result = viewportRect;
        result.setX(result.x() * ratio);
        result.setWidth(result.width() * ratio);
        return result;
    }

    return viewportRect;
}

QOpenGLContext* QnVideoOutputBackend::glContext() const
{
    Q_D(const QnVideoOutputBackend);
    return d->glContext;
}

void QnVideoOutputBackend::present(const QVideoFrame& frame)
{
    Q_D(QnVideoOutputBackend);

    d->frameMutex.lock();
    d->frame = frame;
    d->frameChanged = true;
    d->frameMutex.unlock();

    d->videoOutput->update();
}

void QnVideoOutputBackend::stop()
{
    present(QVideoFrame());
}

QList<QSGVideoNodeFactoryInterface*> QnVideoOutputBackend::videoNodeFactories() const
{
    Q_D(const QnVideoOutputBackend);
    return d->videoNodeFactories;
}
