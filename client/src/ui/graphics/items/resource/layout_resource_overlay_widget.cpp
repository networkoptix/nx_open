#include "layout_resource_overlay_widget.h"

#include <QtCore/QMimeData>
#include <QtGui/QDrag>

#include <common/common_globals.h>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/videowall_resource_screen_widget.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_resource.h>

#include <utils/common/scoped_painter_rollback.h>

QnLayoutResourceOverlayWidget::QnLayoutResourceOverlayWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    m_widget(parent),
    m_videowall(videowall),
    m_itemUuid(itemUuid)
{
    setAcceptDrops(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);

    connect(m_videowall, &QnVideoWallResource::itemChanged, this, &QnLayoutResourceOverlayWidget::at_videoWall_itemChanged);
    connect(this, &QnClickableWidget::doubleClicked, this, &QnLayoutResourceOverlayWidget::at_doubleClicked);

    m_hoverProcessor = new HoverFocusProcessor(this);
    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverEnterDelay(50);
    m_hoverProcessor->setHoverLeaveDelay(50);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverEntered, this, &QnLayoutResourceOverlayWidget::updateView);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this, &QnLayoutResourceOverlayWidget::updateView);

    updateLayout();
    updateInfo();
}


void QnLayoutResourceOverlayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    QRectF paintRect = option->rect;
    if (!paintRect.isValid())
        return;

    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
    painter->fillRect(paintRect, Qt::black);
    paintRect.adjust(offset, offset, -offset, -offset);
    if (m_hoverProcessor->isHovered())
        painter->fillRect(paintRect, Qt::blue);
    else
        painter->fillRect(paintRect, Qt::lightGray);
    paintRect.adjust(offset, offset, -offset, -offset);
    painter->fillRect(paintRect, Qt::black);

    if (!m_layout) {
        //TODO #GDM VW add status overlay widgets
        painter->drawPixmap(paintRect, qnSkin->pixmap("events/thumb_no_data.png").scaled(paintRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation), QRectF());
    }
    else {
        //TODO: #GDM VW paint layout background and calculate its size in bounding geometry
        QRectF bounding;
        foreach (const QnLayoutItemData &data, m_layout->getItems()) {
            QRectF itemRect = data.combinedGeometry;
            if (!itemRect.isValid())
                continue; //TODO: #GDM VW some items can be not placed yet, wtf
            bounding = bounding.united(itemRect);
        }

        if (bounding.isNull())
            return;

        qreal xspace = m_layout->cellSpacing().width() * 0.5;
        qreal yspace = m_layout->cellSpacing().height() * 0.5;

        qreal xscale, yscale, xoffset, yoffset;
        qreal sourceAr = m_layout->cellAspectRatio() * bounding.width() / bounding.height();
        qreal targetAr = paintRect.width() / paintRect.height();
        if (sourceAr > targetAr) {
            xscale = paintRect.width() / bounding.width();
            yscale = xscale / m_layout->cellAspectRatio();
            xoffset = paintRect.left();

            qreal h = bounding.height() * yscale;
            yoffset = (paintRect.height() - h) * 0.5 + paintRect.top();
        } else {
            yscale = paintRect.height() / bounding.height();
            xscale = yscale * m_layout->cellAspectRatio();
            yoffset = paintRect.top();

            qreal w = bounding.width() * xscale;
            xoffset = (paintRect.width() - w) * 0.5 + paintRect.left();
        }

#ifdef RECT_DEBUG
        QPen pen(Qt::red);
        pen.setWidth(15);
        painter->setPen(pen);
#endif

        foreach (const QnLayoutItemData &data, m_layout->getItems()) {
            QRectF itemRect = data.combinedGeometry;
            if (!itemRect.isValid())
                continue;

            // cell bounds
            qreal x1 = (itemRect.left() - bounding.left() + xspace) * xscale + xoffset;
            qreal y1 = (itemRect.top() - bounding.top() + yspace) * yscale + yoffset;
            qreal w1 = (itemRect.width() - xspace*2) * xscale;
            qreal h1 = (itemRect.height() - yspace*2) * yscale;

            paintItem(painter, QRectF(x1, y1, w1, h1), data);
#ifdef RECT_DEBUG
            painter->drawRect(QRectF(x1, y1, w1, h1));
#endif
        }
    }
}

void QnLayoutResourceOverlayWidget::dragEnterEvent(QGraphicsSceneDragDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();

    QnResourceList resources = QnWorkbenchResource::deserializeResources(mimeData);
    QnResourceList media;
    QnResourceList layouts;
    QnResourceList servers;

    m_dragged.resources.clear();
    m_dragged.videoWallItems.clear();

    foreach( QnResourcePtr res, resources )
    {
        if( dynamic_cast<QnMediaResource*>(res.data()) )
            media.push_back( res );
        if( res.dynamicCast<QnLayoutResource>() )
            layouts.push_back( res );
        if( res.dynamicCast<QnMediaServerResource>() )
            servers.push_back( res );
    }

//    if (layouts.size() > 1)
//        return;

//    if (layouts.size() == 1 && (media.size() > 0 || servers.size() > 0))
//        return;

    m_dragged.resources = media;
    m_dragged.resources << layouts;
    m_dragged.resources << servers;

    m_dragged.videoWallItems = qnResPool->getVideoWallItemsByUuid(QnVideoWallItem::deserializeUuids(mimeData));

    if (m_dragged.resources.empty() && m_dragged.videoWallItems.empty())
        return;

    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::dragMoveEvent(QGraphicsSceneDragDropEvent *event) {
    if(m_dragged.resources.empty() && m_dragged.videoWallItems.empty())
        return;

    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent *event) {
//    if(m_dragged.resources.empty() && m_dragged.videoWallItems.empty())
//        return;

//    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::dropEvent(QGraphicsSceneDragDropEvent *event) {

    //TODO: #GDM VW permissions check
    QnLayoutResourceList layouts = m_dragged.resources.filtered<QnLayoutResource>();

    QnVideoWallItemIndexList indexes;
    indexes << QnVideoWallItemIndex(m_videowall, m_itemUuid);

    if (!m_dragged.videoWallItems.isEmpty()) {
        QnVideoWallItemIndex idx = m_dragged.videoWallItems.first();
        QnVideoWallItem item = m_videowall->getItem(m_itemUuid);
        item.layout = idx.videowall()->getItem(idx.uuid()).layout;
        m_videowall->updateItem(m_itemUuid, item);
        //TODO: #GDM do through action to save the videowall at once
    } else if (!layouts.isEmpty()) {
        //TODO: #GDM VW combine layouts?
        menu()->trigger(Qn::ResetVideoWallLayoutAction, QnActionParameters(indexes).withArgument(Qn::LayoutResourceRole, layouts.first()));
    } else {
        //TODO: #GDM VW create a layout
    }

    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::pressedNotify(QGraphicsSceneMouseEvent *event) {
    if (event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
        //TODO #GDM VW: hack because we do not receive mouseReleaseEvent
        emit doubleClicked(Qt::LeftButton);
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();

    QnVideoWallItem::serializeUuids(QList<QUuid>() << m_itemUuid, mimeData);
    mimeData->setData(Qn::NoSceneDrop, QByteArray());

    drag->setMimeData(mimeData);
    drag->exec();
}

void QnLayoutResourceOverlayWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.uuid != m_itemUuid)
        return;
    updateLayout();
    updateInfo();
}

void QnLayoutResourceOverlayWidget::at_doubleClicked(Qt::MouseButton button) {
    Q_UNUSED(button)
    menu()->trigger(
        Qn::StartVideoWallControlAction,
        QnActionParameters(QnVideoWallItemIndexList() << QnVideoWallItemIndex(m_videowall, m_itemUuid))
    );
}

void QnLayoutResourceOverlayWidget::updateLayout() {
    QnVideoWallItem item = m_videowall->getItem(m_itemUuid);
    QnLayoutResourcePtr layout = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
    if (m_layout == layout)
        return;

    if (m_layout) {
        disconnect(m_layout, NULL, this, NULL);
    }
    m_layout = layout;
    if (m_layout) {
        connect(m_layout, &QnLayoutResource::itemAdded,                 this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::itemChanged,               this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::itemRemoved,               this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::cellAspectRatioChanged,    this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::cellSpacingChanged,        this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundImageChanged,    this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundSizeChanged,     this, &QnLayoutResourceOverlayWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundOpacityChanged,  this, &QnLayoutResourceOverlayWidget::updateView);

        connect(m_layout, &QnResource::nameChanged,                     this, &QnLayoutResourceOverlayWidget::updateInfo);
    }
}

void QnLayoutResourceOverlayWidget::updateView() {
    update(); //direct connect is not so convinient because of overloaded function
}

void QnLayoutResourceOverlayWidget::updateInfo() {
    //TODO: #GDM VW update title text

    qDebug() << "info updated";
}

void QnLayoutResourceOverlayWidget::paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data) {
    QnResourcePtr resource = (data.resource.id.isValid())
            ? qnResPool->getResourceById(data.resource.id)
            : qnResPool->getResourceByUniqId(data.resource.path);

    bool isServer = resource && (resource->flags() & QnResource::server);

    if (isServer && !m_widget->m_thumbs.contains(resource->getId())) {
        m_widget->m_thumbs[resource->getId()] = qnSkin->pixmap("events/thumb_server.png");
    } //TODO: #GDM VW local files placeholder

    if (resource && m_widget->m_thumbs.contains(resource->getId())) {
        QPixmap pixmap = m_widget->m_thumbs[resource->getId()];


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
        m_widget->m_thumbnailManager->selectResource(resource);
    }

    {
        //            QnScopedPainterPenRollback(painter, QPen(Qt::gray, 15));
        //            painter->drawRect(paintRect);
    }
}

