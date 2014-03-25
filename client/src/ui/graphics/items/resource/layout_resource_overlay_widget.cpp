#include "layout_resource_overlay_widget.h"

#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtWidgets/QApplication>

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
#include <ui/animation/variant_animator.h>
#include <ui/graphics/items/resource/videowall_resource_screen_widget.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/processors/drag_processor.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_resource.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/linear_combination.h>

class QnLayoutResourceOverlayWidgetHoverProgressAccessor: public AbstractAccessor {
    virtual QVariant get(const QObject *object) const override {
        return static_cast<const QnLayoutResourceOverlayWidget *>(object)->m_hoverProgress;
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        QnLayoutResourceOverlayWidget *widget = static_cast<QnLayoutResourceOverlayWidget *>(object);
        if(qFuzzyCompare(widget->m_hoverProgress, value.toReal()))
            return;

        widget->m_hoverProgress = value.toReal();
        widget->update();
    }
};

QnLayoutResourceOverlayWidget::QnLayoutResourceOverlayWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    m_widget(parent),
    m_videowall(videowall),
    m_itemUuid(itemUuid),
    m_indices(QnVideoWallItemIndexList() << QnVideoWallItemIndex(m_videowall, m_itemUuid)),
    m_hoverProgress(0.0)
{
    setAcceptDrops(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setClickableButtons(Qt::LeftButton | Qt::RightButton);

    connect(m_videowall, &QnVideoWallResource::itemChanged, this, &QnLayoutResourceOverlayWidget::at_videoWall_itemChanged);
    connect(this, &QnClickableWidget::doubleClicked, this, &QnLayoutResourceOverlayWidget::at_doubleClicked);

    m_dragProcessor = new DragProcessor(this);
    m_dragProcessor->setHandler(this);

    m_frameColorAnimator = new VariantAnimator(this);
    m_frameColorAnimator->setTargetObject(this);
    m_frameColorAnimator->setAccessor(new QnLayoutResourceOverlayWidgetHoverProgressAccessor());
    m_frameColorAnimator->setSpeed(1000.0 / qnGlobals->opacityChangePeriod());
    registerAnimation(m_frameColorAnimator);

    m_hoverProcessor = new HoverFocusProcessor(this);
    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverEnterDelay(50);
    m_hoverProcessor->setHoverLeaveDelay(50);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverEntered,   this, [&](){ m_frameColorAnimator->animateTo(1.0); });
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft,      this, [&](){ m_frameColorAnimator->animateTo(0.0); });

    updateLayout();
    updateInfo();
}

const QnResourceWidgetFrameColors &QnLayoutResourceOverlayWidget::frameColors() const {
    return m_frameColors;
}

void QnLayoutResourceOverlayWidget::setFrameColors(const QnResourceWidgetFrameColors &frameColors) {
    m_frameColors = frameColors;
    update();
}


void QnLayoutResourceOverlayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    QRectF paintRect = option->rect;
    if (!paintRect.isValid())
        return;

    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
    paintRect.adjust(offset*2, offset*2, -offset*2, -offset*2);
    painter->fillRect(paintRect, palette().color(QPalette::Window));

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
        }
    }

    paintFrame(painter, paintRect);
}

void QnLayoutResourceOverlayWidget::paintFrame(QPainter *painter, const QRectF &paintRect) {
    if (!paintRect.isValid())
        return;

    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
    qreal m_frameOpacity = 1.0;
    qreal m_frameWidth = offset;

    QColor color = linearCombine(m_hoverProgress, m_frameColors.selected, 1 - m_hoverProgress, m_frameColors.normal);

    qreal w = paintRect.width();
    qreal h = paintRect.height();
    qreal fw = m_frameWidth;
    qreal x = paintRect.x();
    qreal y = paintRect.y();

    QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_frameOpacity);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
    painter->fillRect(QRectF(x - fw,    y - fw,  w + fw * 2,  fw), color);
    painter->fillRect(QRectF(x - fw,    y + h,   w + fw * 2,  fw), color);
    painter->fillRect(QRectF(x - fw,    y,       fw,          h),  color);
    painter->fillRect(QRectF(x + w,     y,       fw,          h),  color);

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

    m_hoverProcessor->forceHoverEnter();
    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent *event) {
    m_hoverProcessor->forceHoverLeave();
}

void QnLayoutResourceOverlayWidget::dropEvent(QGraphicsSceneDragDropEvent *event) {

    QnActionParameters parameters;
    if (!m_dragged.videoWallItems.isEmpty())
        parameters = QnActionParameters(m_dragged.videoWallItems);
    else
        parameters = QnActionParameters(m_dragged.resources);
    parameters.setArgument(Qn::VideoWallItemGuidRole, m_itemUuid);
    menu()->trigger(Qn::DropOnVideoWallItemAction, parameters);

    event->acceptProposedAction();
}

void QnLayoutResourceOverlayWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    m_dragProcessor->mousePressEvent(this, event);
}


void QnLayoutResourceOverlayWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseMoveEvent(event);
    m_dragProcessor->mouseMoveEvent(this, event);
}

void QnLayoutResourceOverlayWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    m_dragProcessor->mouseReleaseEvent(this, event);
}

void QnLayoutResourceOverlayWidget::startDrag(DragInfo *info) {
    Q_UNUSED(info)

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();

    QnVideoWallItem::serializeUuids(QList<QUuid>() << m_itemUuid, mimeData);
    mimeData->setData(Qn::NoSceneDrop, QByteArray());

    drag->setMimeData(mimeData);
    drag->exec();

    m_dragProcessor->reset();
}

void QnLayoutResourceOverlayWidget::clickedNotify(QGraphicsSceneMouseEvent *event) {
    if (event->button() != Qt::RightButton)
        return;

    QScopedPointer<QMenu> popupMenu(menu()->newMenu(Qn::SceneScope, mainWindow(), QnActionParameters(m_indices)));
    if(popupMenu->isEmpty())
        return;

    popupMenu->exec(event->screenPos());
}

void QnLayoutResourceOverlayWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.uuid != m_itemUuid)
        return;
    updateLayout();
    updateInfo();
}

void QnLayoutResourceOverlayWidget::at_doubleClicked(Qt::MouseButton button) {
    if (button != Qt::LeftButton)
        return;

    menu()->trigger(
        Qn::StartVideoWallControlAction,
        QnActionParameters(m_indices)
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

