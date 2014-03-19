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
#include <ui/graphics/items/resource/layout_resource_overlay_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/container.h>

QnVideowallResourceScreenWidget::QnVideowallResourceScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent),
    m_mainLayout(NULL),
    m_layoutUpdateRequired(true)
{
    setOption(QnResourceWidget::WindowRotationForbidden, true);

    m_videowall = base_type::resource().dynamicCast<QnVideoWallResource>();
    if(!m_videowall) {
        qnCritical("QnVideowallResourceScreenWidget was created with a non-videowall resource.");
        return;
    }

    connect(m_videowall, &QnVideoWallResource::itemAdded,     this, &QnVideowallResourceScreenWidget::at_videoWall_itemAdded);
    connect(m_videowall, &QnVideoWallResource::itemChanged,   this, &QnVideowallResourceScreenWidget::at_videoWall_itemChanged);
    connect(m_videowall, &QnVideoWallResource::itemRemoved,   this, &QnVideowallResourceScreenWidget::at_videoWall_itemRemoved);

    m_pcUuid = item->data(Qn::VideoWallPcGuidRole).value<QUuid>();
    if (!m_videowall->hasPc(m_pcUuid))
        return;

    QnVideoWallPcData pc = m_videowall->getPc(m_pcUuid);
    QList<int> screenIndices = item->data(Qn::VideoWallPcScreenIndicesRole).value<QList<int> >();

    foreach(const QnVideoWallPcData::PcScreen &screen, pc.screens) {
        if (!screenIndices.contains(screen.index))
            continue;
        m_screens << screen;
        m_desktopGeometry = m_desktopGeometry.united(screen.desktopGeometry);
    }

    if (m_desktopGeometry.isValid())
        setAspectRatio((qreal)m_desktopGeometry.width() / m_desktopGeometry.height());

    foreach (const QnVideoWallItem &item, m_videowall->getItems())
        at_videoWall_itemAdded(m_videowall, item);

    m_thumbnailManager = context->instance<QnCameraThumbnailManager>();
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
    painter->fillRect(paintRect, Qt::transparent);
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
            QnLayoutResourceOverlayWidget *itemWidget = new QnLayoutResourceOverlayWidget(m_videowall, item.uuid, this);
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
        QnLayoutResourceOverlayWidget *itemWidget = new QnLayoutResourceOverlayWidget(m_videowall, m_items.first().uuid, this);
        itemWidget->setAcceptedMouseButtons(Qt::NoButton);
        m_mainLayout->addAnchors(itemWidget, m_mainLayout, Qt::Horizontal | Qt::Vertical);
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
