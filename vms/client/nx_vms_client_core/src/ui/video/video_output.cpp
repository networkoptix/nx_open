#include "video_output.h"

#include <QtCore/QPointer>

#include <nx/media/media_player.h>
#include "video_output_backend.h"

#include <nx/utils/log/assert.h>

class QnVideoOutputPrivate: public QObject
{
    QnVideoOutput* q_ptr;
    Q_DECLARE_PUBLIC(QnVideoOutput)

public:
    QnVideoOutputPrivate(QnVideoOutput* parent);

    bool createBackend();
    void updateNativeSize();
    void updateGeometry();

public:
    QPointer<nx::media::Player> player;

    QnVideoOutput::FillMode fillMode = QnVideoOutput::PreserveAspectFit;
    QSize nativeSize;

    bool geometryDirty = true;
    QRectF lastRect;      // Cache of last rect to avoid recalculating geometry
    QRectF contentRect;   // Destination pixel coordinates, unclipped

    QScopedPointer<QnVideoOutputBackend> backend;
};

QnVideoOutputPrivate::QnVideoOutputPrivate(QnVideoOutput* parent):
    QObject(parent),
    q_ptr(parent)
{
}

bool QnVideoOutputPrivate::createBackend()
{
    Q_Q(QnVideoOutput);

    if (!backend)
        backend.reset(new QnVideoOutputBackend(q));

    backend->updateGeometry();

    return true;
}

void QnVideoOutputPrivate::updateNativeSize()
{
    Q_Q(QnVideoOutput);

    if (!backend)
        return;

    const auto size = backend->nativeSize();

    if (nativeSize != size) {
        nativeSize = size;

        geometryDirty = true;

        q->setImplicitWidth(size.width());
        q->setImplicitHeight(size.height());

        emit q->sourceRectChanged();
    }
}

void QnVideoOutputPrivate::updateGeometry()
{
    Q_Q(QnVideoOutput);

    const auto rect = QRectF(0, 0, q->width(), q->height());
    const auto absoluteRect = QRectF(q->x(), q->y(), rect.width(), rect.height());

    if (!geometryDirty && lastRect == absoluteRect)
        return;

    auto oldContentRect = contentRect;

    geometryDirty = false;
    lastRect = absoluteRect;

    if (nativeSize.isEmpty())
    {
        //this is necessary for item to receive the
        //first paint event and configure video surface.
        contentRect = rect;
    }
    else if (fillMode == QnVideoOutput::Stretch)
    {
        contentRect = rect;
    }
    else if (fillMode == QnVideoOutput::PreserveAspectFit
        || fillMode == QnVideoOutput::PreserveAspectCrop)
    {
        auto scaled = nativeSize;
        scaled.scale(
            rect.size().toSize(),
            fillMode == QnVideoOutput::PreserveAspectFit
                ? Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);

        contentRect = QRectF(QPointF(), scaled);
        contentRect.moveCenter(rect.center());
    }

    if (backend)
        backend->updateGeometry();

    if (contentRect != oldContentRect)
        emit q->contentRectChanged();
}

QnVideoOutput::QnVideoOutput(QQuickItem* parent):
    base_type(parent),
    d_ptr(new QnVideoOutputPrivate(this))
{
    setFlag(ItemHasContents, true);
}

QnVideoOutput::~QnVideoOutput()
{
    Q_D(QnVideoOutput);

    d->backend.reset();
    d->player.clear();
}

nx::media::Player* QnVideoOutput::player() const
{
    Q_D(const QnVideoOutput);
    return d->player;
}

void QnVideoOutput::setPlayer(nx::media::Player* player, int channel)
{
    Q_D(QnVideoOutput);

    if (d->player == player)
        return;

    if (d->backend)
        d->backend->releasePlayer();

    d->player = player;

    if (player)
    {
        d->backend.reset();
        d->createBackend();
        d->player->setVideoSurface(d->backend->videoSurface(), channel);
    }

    emit playerChanged();
}

QnVideoOutput::FillMode QnVideoOutput::fillMode() const
{
    Q_D(const QnVideoOutput);
    return d->fillMode;
}

void QnVideoOutput::setFillMode(FillMode mode)
{
    Q_D(QnVideoOutput);

    if (d->fillMode == mode)
        return;

    d->fillMode = mode;
    d->geometryDirty = true;

    update();

    emit fillModeChanged(mode);
}

QRectF QnVideoOutput::contentRect() const
{
    Q_D(const QnVideoOutput);
    return d->contentRect;
}

void QnVideoOutput::updateNativeSize()
{
    Q_D(QnVideoOutput);
    d->updateNativeSize();
}

Q_INVOKABLE void QnVideoOutput::clear()
{
    Q_D(QnVideoOutput);
    if (d->backend)
        d->backend->present(QVideoFrame());
}

QRectF QnVideoOutput::sourceRect() const
{
    Q_D(const QnVideoOutput);

    // We might have to transpose back
    auto size = d->nativeSize;

    // No backend? Just assume no viewport.
    if (!d->nativeSize.isValid() || !d->backend)
        return QRectF(QPointF(), size);

    // Take the viewport into account for the top left position.
    // m_nativeSize is already adjusted to the viewport, as it originats
    // from QVideoSurfaceFormat::sizeHint(), which includes pixel aspect
    // ratio and viewport.
    const auto viewportRect = d->backend->adjustedViewport();
    NX_ASSERT(viewportRect.size() == size);
    return QRectF(viewportRect.topLeft(), size);
}

QSGNode* QnVideoOutput::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    Q_D(QnVideoOutput);

    d->updateGeometry();

    if (!d->backend)
        return nullptr;

    return d->backend->updatePaintNode(oldNode, data);
}

void QnVideoOutput::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    // Explicitly listen to geometry changes here. This is needed since changing the position does
    // not trigger a call to updatePaintNode().
    // We need to react to position changes though, as the window backened's display rect gets
    // changed in that situation.
    Q_D(QnVideoOutput);
    d->updateGeometry();
}
