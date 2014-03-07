#include "layout_resource_widget.h"

#include <QtGui/QPainter>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>

//#define RECT_DEBUG

QnLayoutResourceWidget::QnLayoutResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent)
{
    m_resource = base_type::resource().dynamicCast<QnLayoutResource>();
    if(!m_resource)
        qnCritical("Layout resource widget was created with a non-layout resource.");

    m_thumbnailManager = new QnCameraThumbnailManager(this);
    connect(m_thumbnailManager, SIGNAL(thumbnailReady(int,QPixmap)), this, SLOT(at_thumbnailReady(int, QPixmap)));

    setOption(QnResourceWidget::WindowRotationForbidden, true);

    updateButtonsVisibility();
    updateTitleText();
    updateInfoText();
}

QnLayoutResourceWidget::~QnLayoutResourceWidget() {
    return;
}

const QnLayoutResourcePtr &QnLayoutResourceWidget::resource() const {
    return m_resource;
}

QnVideoWallItemIndexList QnLayoutResourceWidget::linkedVideoWalls() const {

    QnVideoWallResourcePtr itemVideoWall = item()->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>();
    QUuid itemUuid = item()->data(Qn::VideoWallItemGuidRole).value<QUuid>();
    if (itemVideoWall && !itemUuid.isNull())
        return QnVideoWallItemIndexList() << QnVideoWallItemIndex(itemVideoWall, itemUuid);

    QnVideoWallItemIndexList indexes;
    QnId layoutId = m_resource->getId();
    foreach (const QnVideoWallResourcePtr &videoWall, qnResPool->getResources().filtered<QnVideoWallResource>()) {
        foreach (const QnVideoWallItem &item, videoWall->getItems()) {
            if (item.layout != layoutId)
                continue;
            indexes << QnVideoWallItemIndex(videoWall, item.uuid);
        }
    }
    return indexes;
}

void QnLayoutResourceWidget::paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data) {
    QnResourcePtr resource = (data.resource.id.isValid())
            ? qnResPool->getResourceById(data.resource.id)
            : qnResPool->getResourceByUniqId(data.resource.path);

    if (resource && m_thumbs.contains(resource->getId())) {
        QPixmap pixmap = m_thumbs[resource->getId()];

        bool isServer = resource && (resource->flags() & QnResource::server);

        qreal targetAr = paintRect.width() / paintRect.height();
        qreal sourceAr = isServer
                ? targetAr
                : (qreal)pixmap.width() / pixmap.height();

        qreal x, y, w, h;
        if (sourceAr > targetAr) {
            w = paintRect.width();
            x = paintRect.left();
            h = w / sourceAr;
            y = (paintRect.height() - h) * 0.5 + paintRect.top();
        } else {
            h = paintRect.height();
            y = paintRect.top();
            w = h * sourceAr;
            x = (paintRect.width() - w) * 0.5 + paintRect.left();
        }

        if (!qFuzzyIsNull(data.rotation)) {
            QnScopedPainterTransformRollback guard(painter); Q_UNUSED(guard);
            painter->translate(paintRect.center());
            painter->rotate(data.rotation);
            painter->translate(-paintRect.center());
            painter->drawPixmap(QRectF(x, y, w, h).toRect(), pixmap);
        } else {
            painter->drawPixmap(QRectF(x, y, w, h).toRect(), pixmap);
        }
        return;
    }

    if (resource && (resource->flags() & QnResource::live_cam) && resource.dynamicCast<QnNetworkResource>()) {
        m_thumbnailManager->selectResource(resource);
    } else if (resource && (resource->flags() & QnResource::server)) {
        m_thumbs[resource->getId()] = qnSkin->pixmap("events/thumb_server.png");
    } //TODO: #GDM VW local files placeholder

    {
        QnScopedPainterPenRollback(painter, QPen(Qt::gray, 15));
        painter->drawRect(paintRect);
    }
}

Qn::RenderStatus QnLayoutResourceWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {

    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    if (!paintRect.isValid())
        return Qn::NothingRendered;

    painter->fillRect(paintRect, Qt::black);
    {
        QnScopedPainterPenRollback(painter, QPen(Qt::blue, 15));
        painter->drawRect(paintRect);
    }

    if (m_resource->getItems().isEmpty()) {
        return Qn::NothingRendered;
    }

    QRectF bounding;
    foreach (const QnLayoutItemData &data, m_resource->getItems()) {
        QRectF itemRect = data.combinedGeometry;
        if (!itemRect.isValid())
            continue;
        bounding = bounding.united(itemRect);
    }

    if (bounding.isNull()) {
        return Qn::NothingRendered;
    }

    qreal sourceAr = m_resource->cellAspectRatio() * bounding.width() / bounding.height();
    setAspectRatio(sourceAr);

    qreal x = paintRect.left();
    qreal y = paintRect.top();

    qreal xspace = m_resource->cellSpacing().width() * 0.5;
    qreal yspace = m_resource->cellSpacing().height() * 0.5;

    qreal xscale = paintRect.width() / bounding.width();
    qreal yscale = xscale / m_resource->cellAspectRatio();

#ifdef RECT_DEBUG
    QPen pen(Qt::red);
    pen.setWidth(15);
    painter->setPen(pen);
#endif

    foreach (const QnLayoutItemData &data, m_resource->getItems()) {
        QRectF itemRect = data.combinedGeometry;
        if (!itemRect.isValid())
            continue;

        qreal x1 = (itemRect.left() - bounding.left() + xspace) * xscale;
        qreal y1 = (itemRect.top() - bounding.top() + yspace) * yscale;
        qreal w1 = (itemRect.width() - xspace*2) * xscale;
        qreal h1 = (itemRect.height() - yspace*2) * yscale;

        paintItem(painter, QRectF(x + x1, y + y1, w1, h1), data);
#ifdef RECT_DEBUG
        painter->drawRect(QRectF(x + x1, y + y1, w1, h1));
#endif
    }

    return Qn::NewFrameRendered;
}

void QnLayoutResourceWidget::at_thumbnailReady(int resourceId, const QPixmap &thumbnail) {
    m_thumbs[resourceId] = thumbnail;
    update();
}

QString QnLayoutResourceWidget::calculateTitleText() const {
    QnVideoWallItemIndexList indexes = linkedVideoWalls();
    if (indexes.isEmpty())
        return base_type::calculateTitleText();

    QStringList itemNames;
    foreach (const QnVideoWallItemIndex &index, indexes) {
        itemNames << index.videowall()->getItem(index.uuid()).name;
    }
    return itemNames.join(QLatin1String(", "));
}


QString QnLayoutResourceWidget::calculateInfoText() const {
    QnVideoWallItemIndexList indexes = linkedVideoWalls();
    if (indexes.isEmpty())
        return tr("No videowall screens contain this layout.");

    QnVideoWallResourceList videowalls;
    foreach (const QnVideoWallItemIndex &index, indexes) {
        if (!videowalls.contains(index.videowall()))
            videowalls << index.videowall();
    }

    if (videowalls.size() == 1)
        return m_resource->getName();

    QStringList videoWallNames;
    foreach (const QnVideoWallResourcePtr &videowall, videowalls)
        videoWallNames << videowall->getName();

    return QString(QLatin1String("%1\t%2"))
            .arg(m_resource->getName())
            .arg(videoWallNames.join(QLatin1String(", ")));
}

Qn::ResourceStatusOverlay QnLayoutResourceWidget::calculateStatusOverlay() const {
    return Qn::EmptyOverlay;
}
