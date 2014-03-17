#include "videowall_resource_screen_widget.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsAnchorLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/container.h>
#include <utils/common/scoped_painter_rollback.h>


// -------------------------------------------------------------------------- //
// StatisticsOverlayWidget
// -------------------------------------------------------------------------- //
class LayoutOverlayWidget: public Connective<GraphicsWidget> {
    typedef Connective<GraphicsWidget> base_type;
public:
    LayoutOverlayWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_widget(parent),
        m_videowall(videowall),
        m_itemUuid(itemUuid)
    {
        connect(m_videowall, &QnVideoWallResource::itemChanged, this, &LayoutOverlayWidget::at_videoWall_itemChanged);

        updateLayout();
        updateInfo();
    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(widget)
        QRectF paintRect = option->rect;
        if (!paintRect.isValid())
            return;

        qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
        painter->fillRect(paintRect, Qt::black);
        paintRect.adjust(offset, offset, -offset, -offset);
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

            qreal x = paintRect.left();
            qreal y = paintRect.top();

            qreal xspace = m_layout->cellSpacing().width() * 0.5;
            qreal yspace = m_layout->cellSpacing().height() * 0.5;

            qreal xscale, yscale, xoffset, yoffset;
            qreal sourceAr = m_layout->cellAspectRatio() * bounding.width() / bounding.height();
            qreal targetAr = paintRect.width() / paintRect.height();
            if (sourceAr > targetAr) {
                xscale = paintRect.width() / bounding.width();
                yscale = xscale / m_layout->cellAspectRatio();
                xoffset = 0;

                qreal h = bounding.height() * yscale;
                yoffset = (paintRect.height() - h) * 0.5 + paintRect.top();
            } else {
                yscale = paintRect.height() / bounding.height();
                xscale = yscale * m_layout->cellAspectRatio();
                yoffset = 0;

                qreal w = bounding.width() * xscale;
                xoffset = (paintRect.width() - w) * 0.5 + paintRect.top();
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

                paintItem(painter, QRectF(x + x1, y + y1, w1, h1), data);
        #ifdef RECT_DEBUG
                painter->drawRect(QRectF(x + x1, y + y1, w1, h1));
        #endif
            }
        }
    }

private:
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
        Q_UNUSED(videoWall)
        if (item.uuid != m_itemUuid)
            return;
        updateLayout();
        updateInfo();
    }

    void updateLayout() {
        QnVideoWallItem item = m_videowall->getItem(m_itemUuid);
        QnLayoutResourcePtr layout = qnResPool->getResourceById(item.layout).dynamicCast<QnLayoutResource>();
        if (m_layout == layout)
            return;

        if (m_layout) {
            disconnect(m_layout, NULL, this, NULL);
        }
        m_layout = layout;
        if (m_layout) {
            connect(m_layout, &QnLayoutResource::itemAdded,                 this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::itemChanged,               this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::itemRemoved,               this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::cellAspectRatioChanged,    this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::cellSpacingChanged,        this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::backgroundImageChanged,    this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::backgroundSizeChanged,     this, &LayoutOverlayWidget::updateView);
            connect(m_layout, &QnLayoutResource::backgroundOpacityChanged,  this, &LayoutOverlayWidget::updateView);

            connect(m_layout, &QnResource::nameChanged,                     this, &LayoutOverlayWidget::updateInfo);
        }
    }

    void updateView() {
        update(); //direct connect is not so convinient because of overloaded function
    }

    void updateInfo() {
        //TODO: #GDM VW update title text

        qDebug() << "info updated";
    }

    void paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data) {
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

private:
    QnVideowallResourceScreenWidget* m_widget;

    const QnVideoWallResourcePtr m_videowall;
    const QUuid m_itemUuid;

    QnLayoutResourcePtr m_layout;
};


// -------------------------------------------------------------------------- //
// QnVideowallResourceScreenWidget
// -------------------------------------------------------------------------- //

QnVideowallResourceScreenWidget::QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent),
    m_mainLayout(NULL),
    m_layoutUpdateRequired(true)
{
    setOption(QnResourceWidget::WindowRotationForbidden, true);

    m_videowall = base_type::resource().dynamicCast<QnVideoWallResource>(); //TODO: #GDM VW check null in all usage places
    if(!m_videowall) {
        qnCritical("QnVideowallResourceScreenWidget was created with a non-videowall resource.");
        return;
    }

    connect(m_videowall, &QnVideoWallResource::itemAdded,     this, &QnVideowallResourceScreenWidget::at_videoWall_itemAdded);
    connect(m_videowall, &QnVideoWallResource::itemChanged,   this, &QnVideowallResourceScreenWidget::at_videoWall_itemChanged);
    connect(m_videowall, &QnVideoWallResource::itemRemoved,   this, &QnVideowallResourceScreenWidget::at_videoWall_itemRemoved);


    m_pcUuid = item->data(Qn::VideoWallPcGuidRole).value<QUuid>();
    QnVideoWallPcData pc = m_videowall->getPc(m_pcUuid);  //TODO: #GDM VW check null in all usage places

    QList<int> screenIndices = item->data(Qn::VideoWallPcScreenIndicesRole).value<QList<int> >();

    //TODO: #GDM VW if pc list updated or a single pc changed, videowall review layout must be invalidated

    foreach(const QnVideoWallPcData::PcScreen &screen, pc.screens) {
        if (!screenIndices.contains(screen.index))
            continue;
        m_screens << screen;
        m_desktopGeometry = m_desktopGeometry.united(screen.desktopGeometry);
    }

    if (m_desktopGeometry.isValid())    //TODO: #GDM VW and if not?
        setAspectRatio((qreal)m_desktopGeometry.width() / m_desktopGeometry.height());

    foreach (const QnVideoWallItem &item, m_videowall->getItems()) {
        at_videoWall_itemAdded(m_videowall, item);
    }

    m_thumbnailManager = new QnCameraThumbnailManager(this);
    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this, &QnVideowallResourceScreenWidget::at_thumbnailReady);

    updateButtonsVisibility();
    updateTitleText();
    updateInfoText();
}

QnVideowallResourceScreenWidget::~QnVideowallResourceScreenWidget() {
}

const QnVideoWallResourcePtr &QnVideowallResourceScreenWidget::videowall() const {
    return m_videowall;
}

Qn::RenderStatus QnVideowallResourceScreenWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    if (!paintRect.isValid())
        return Qn::NothingRendered;

    updateLayout();

    painter->fillRect(paintRect, Qt::black);
//    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
//    QRectF contentsRect = paintRect.adjusted(offset, offset, -offset, -offset);
//    painter->fillRect(contentsRect, Qt::black);


    return Qn::NewFrameRendered;
}

void QnVideowallResourceScreenWidget::updateLayout() {
    if (!m_layoutUpdateRequired)
        return;

    if (!m_mainLayout) {
        m_mainLayout = new QGraphicsAnchorLayout();
        m_mainLayout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
        m_mainLayout->setSpacing(0.5);

        QGraphicsWidget* mainOverlayWidget = new QnViewportBoundWidget(this);
        mainOverlayWidget->setLayout(m_mainLayout);
        mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
        mainOverlayWidget->setOpacity(1.0);
        addOverlayWidget(mainOverlayWidget, UserVisible, true);
    }

    while (m_mainLayout->count() > 0) {
        QGraphicsLayoutItem* item = m_mainLayout->itemAt(0);
        m_mainLayout->removeAt(0);
        delete item;
    }

    // can have several items on a single screen
    if (m_screens.size() == 1) {
        foreach (const QnVideoWallItem &item, m_items) {
            LayoutOverlayWidget *itemWidget = new LayoutOverlayWidget(m_videowall, item.uuid, this);
            itemWidget->setAcceptedMouseButtons(Qt::NoButton);

            if (item.geometry.left() == m_desktopGeometry.left())
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorLeft, m_mainLayout, Qt::AnchorLeft);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorLeft, m_mainLayout, Qt::AnchorHorizontalCenter);

            if (item.geometry.top() == m_desktopGeometry.top())
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorTop, m_mainLayout, Qt::AnchorTop);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorTop, m_mainLayout, Qt::AnchorVerticalCenter);

            if (item.geometry.right() == m_desktopGeometry.right())
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorRight, m_mainLayout, Qt::AnchorRight);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorRight, m_mainLayout, Qt::AnchorHorizontalCenter);

            if (item.geometry.bottom() == m_desktopGeometry.bottom())
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorBottom, m_mainLayout, Qt::AnchorBottom);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorBottom, m_mainLayout, Qt::AnchorVerticalCenter);

        }
    } else if (m_items.size() == 1 ) {    // can have only on item on several screens
        LayoutOverlayWidget *itemWidget = new LayoutOverlayWidget(m_videowall, m_items.first().uuid, this);
        itemWidget->setAcceptedMouseButtons(Qt::NoButton);

         //TODO: #GDM VW maybe just place this item as an overlay?
        m_mainLayout->addAnchors(itemWidget, m_mainLayout, Qt::Horizontal | Qt::Vertical);

    } else {
        qWarning() << "inconsistent videowall or no items on screens";
    }




    m_layoutUpdateRequired = false;
}


void QnVideowallResourceScreenWidget::at_thumbnailReady(int resourceId, const QPixmap &thumbnail) {
    m_thumbs[resourceId] = thumbnail;
    update();
}

void QnVideowallResourceScreenWidget::at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.pcUuid != m_pcUuid)
        return;

    if (!m_desktopGeometry.contains(item.geometry))
        return;

    m_items << item;
    m_layoutUpdateRequired = true;
    update();
}

void QnVideowallResourceScreenWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.pcUuid != m_pcUuid)
        return;

    int idx = qnIndexOf(m_items, [&](const QnVideoWallItem &i) {return item.uuid == i.uuid; });
    if (idx < 0)
        return; // item on another screen

    QnVideoWallItem oldItem = m_items[idx];
    m_items[idx] = item;
    if (item.geometry == oldItem.geometry)
       return;

    m_layoutUpdateRequired = true;
    update();
}

void QnVideowallResourceScreenWidget::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.pcUuid != m_pcUuid)
        return;

    int idx = qnIndexOf(m_items, [&](const QnVideoWallItem &i) {return item.uuid == i.uuid; });
    if (idx < 0)
        return; // item on another screen

    m_items.removeAt(idx);
    m_layoutUpdateRequired = true;
    update();
}
