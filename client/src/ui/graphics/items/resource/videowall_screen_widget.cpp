#include "videowall_screen_widget.h"

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
#include <ui/graphics/items/resource/videowall_item_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/container.h>

QnVideowallScreenWidget::QnVideowallScreenWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(context, item, parent),
    m_mainLayout(NULL),
    m_layoutUpdateRequired(true)
{
    setAcceptDrops(true);

    m_videowall = base_type::resource().dynamicCast<QnVideoWallResource>();
    if(!m_videowall) {
        qnCritical("QnVideowallScreenWidget was created with a non-videowall resource.");
        return;
    }

    connect(m_videowall, &QnVideoWallResource::itemAdded,     this, &QnVideowallScreenWidget::at_videoWall_itemAdded);
    connect(m_videowall, &QnVideoWallResource::itemChanged,   this, &QnVideowallScreenWidget::at_videoWall_itemChanged);
    connect(m_videowall, &QnVideoWallResource::itemRemoved,   this, &QnVideowallScreenWidget::at_videoWall_itemRemoved);

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
    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this, &QnVideowallScreenWidget::at_thumbnailReady);

    updateButtonsVisibility();
    updateTitleText();
    updateInfoText();

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    setInfoVisible(true, false);
}

QnVideowallScreenWidget::~QnVideowallScreenWidget() {
}

const QnVideoWallResourcePtr &QnVideowallScreenWidget::videowall() const {
    return m_videowall;
}

Qn::RenderStatus QnVideowallScreenWidget::paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) {
    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    if (!paintRect.isValid())
        return Qn::NothingRendered;

    updateLayout();
    painter->fillRect(paintRect, palette().color(QPalette::Window));
    return Qn::NewFrameRendered;
}

QnResourceWidget::Buttons QnVideowallScreenWidget::calculateButtonsVisibility() const {
    return 0;
}

QString QnVideowallScreenWidget::calculateTitleText() const {
    QStringList pcUuids;
    foreach (const QUuid &uuid, m_videowall->getPcs().keys())
        pcUuids.append(uuid.toString());
    pcUuids.sort(Qt::CaseInsensitive);
    int idx = pcUuids.indexOf(m_pcUuid.toString());
    if (idx < 0)
        idx = 0;

    QString base = tr("Pc %1").arg(idx);
    if (m_screens.isEmpty())
        return base;

    QStringList screenIndices;
    foreach (QnVideoWallPcData::PcScreen screen, m_screens)
        screenIndices.append(QString::number(screen.index));

    return tr("Pc %1 - Screens %2", "", m_screens.size()).arg(idx).arg(screenIndices.join(lit(" ,")));
}

void QnVideowallScreenWidget::updateLayout() {
    if (!m_layoutUpdateRequired)
        return;

    if (!m_mainLayout) {
        m_mainLayout = new QGraphicsAnchorLayout();
        m_mainLayout->setContentsMargins(3.0, 20.0, 3.0, 3.0);
        m_mainLayout->setSpacing(0.0);

        m_mainOverlayWidget = new QnViewportBoundWidget(this);
        m_mainOverlayWidget->setLayout(m_mainLayout);
        m_mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
        m_mainOverlayWidget->setOpacity(1.0);
        addOverlayWidget(m_mainOverlayWidget, UserVisible, true);
    }

    while (m_mainLayout->count() > 0) {
        QGraphicsLayoutItem* item = m_mainLayout->itemAt(0);
        m_mainLayout->removeAt(0);
        delete item;
    }

    // can have several items on a single screen
    if (m_screens.size() == 1) {
        foreach (const QnVideoWallItem &item, m_items) {
            QnVideowallItemWidget *itemWidget = new QnVideowallItemWidget(m_videowall, item.uuid, this, m_mainOverlayWidget);
            itemWidget->setFrameColors(frameColors());

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
        QnVideowallItemWidget *itemWidget = new QnVideowallItemWidget(m_videowall, m_items.first().uuid, this, m_mainOverlayWidget);
        itemWidget->setFrameColors(frameColors());

        m_mainLayout->addAnchors(itemWidget, m_mainLayout, Qt::Horizontal | Qt::Vertical);
    }

    m_layoutUpdateRequired = false;
}


void QnVideowallScreenWidget::at_thumbnailReady(const QnId &resourceId, const QPixmap &thumbnail) {
    m_thumbs[resourceId] = thumbnail;
    update();
}

void QnVideowallScreenWidget::at_videoWall_itemAdded(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)
    if (item.pcUuid != m_pcUuid)
        return;

    if (!m_desktopGeometry.contains(item.geometry))
        return;

    m_items << item;
    m_layoutUpdateRequired = true;
    update();
}

void QnVideowallScreenWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
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

void QnVideowallScreenWidget::at_videoWall_itemRemoved(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
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
