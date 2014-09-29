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
#include <core/resource/videowall_item_index.h>
#include <core/resource/layout_item_data.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/resource/videowall_item_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <utils/common/collection.h>

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

    connect(m_videowall, &QnVideoWallResource::itemChanged,   this, &QnVideowallScreenWidget::at_videoWall_itemChanged);

    m_thumbnailManager = context->instance<QnCameraThumbnailManager>();
    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this, &QnVideowallScreenWidget::at_thumbnailReady);

    updateItems();
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
    if (m_items.isEmpty())
        return QString();

    auto pcUuid = m_videowall->pcs()->getItem(m_items.first().pcUuid).uuid;

    QStringList pcUuids;
    foreach (const QnUuid &uuid, m_videowall->pcs()->getItems().keys())
        pcUuids.append(uuid.toString());
    pcUuids.sort(Qt::CaseInsensitive);
    int idx = pcUuids.indexOf(pcUuid.toString());
    if (idx < 0)
        idx = 0;

    QString base = tr("Pc %1").arg(idx + 1);

    QSet<int> screens = m_items.first().screenSnaps.screens();
    if (screens.isEmpty())
        return base;

    QStringList screenIndices;
    foreach (int screen, screens)
        screenIndices.append(QString::number(screen + 1));

    return tr("Pc %1 - Screens %2", "", screens.size()).arg(idx + 1).arg(screenIndices.join(lit(" ,")));
}

void QnVideowallScreenWidget::updateLayout(bool force) {
    if (!m_layoutUpdateRequired && !force)
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

    ReviewButtons state;
    if (item())
        state = item()->data(Qn::ItemVideowallReviewButtonsRole).value<ReviewButtons>();

    auto createItem = [this, &state](const QnUuid &id) {
        QnVideowallItemWidget *itemWidget = new QnVideowallItemWidget(m_videowall, id, this, m_mainOverlayWidget);
        itemWidget->setFrameColors(frameColors());
        if (state.contains(id))
            itemWidget->setInfoVisible(state[id] > 0, false);

        connect(itemWidget, &QnVideowallItemWidget::infoVisibleChanged, this, [this, id](bool visible){
            if (!this->item())
                return;
            ReviewButtons state = item()->data(Qn::ItemVideowallReviewButtonsRole).value<ReviewButtons>();
            state[id] = visible ? 1 : 0;    //TODO: #VW #temporary solution, will be improved if new buttons will be added
            item()->setData(Qn::ItemVideowallReviewButtonsRole, state);
        });

        return itemWidget;
    };

    
    auto partOfScreen = [](const QnScreenSnaps &snaps) {
        return std::any_of(snaps.values.cbegin(), snaps.values.cend(), [](const QnScreenSnap &snap) {return snap.snapIndex > 0;});
    };

    // can have several items on a single screen - or one item can take just part of the screen
    if (m_items.size() > 1 || (m_items.size() == 1 && partOfScreen(m_items.first().screenSnaps))) {
        foreach (const QnVideoWallItem &item, m_items) {
            QnVideowallItemWidget *itemWidget = createItem(item.uuid);
            assert(QnScreenSnap::snapsPerScreen() == 2);    //in other case layout should be reimplemented

            if (item.screenSnaps.left().snapIndex == 0)
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorLeft, m_mainLayout, Qt::AnchorLeft);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorLeft, m_mainLayout, Qt::AnchorHorizontalCenter);

            if (item.screenSnaps.top().snapIndex == 0)
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorTop, m_mainLayout, Qt::AnchorTop);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorTop, m_mainLayout, Qt::AnchorVerticalCenter);

            if (item.screenSnaps.right().snapIndex == 0)
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorRight, m_mainLayout, Qt::AnchorRight);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorRight, m_mainLayout, Qt::AnchorHorizontalCenter);

            if (item.screenSnaps.bottom().snapIndex == 0)
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorBottom, m_mainLayout, Qt::AnchorBottom);
            else
                m_mainLayout->addAnchor(itemWidget, Qt::AnchorBottom, m_mainLayout, Qt::AnchorVerticalCenter);

        }
    } else if (m_items.size() == 1 ) {    // can have only one item on several screens
        QnVideowallItemWidget *itemWidget = createItem(m_items.first().uuid);
        m_mainLayout->addAnchors(itemWidget, m_mainLayout, Qt::Horizontal | Qt::Vertical);
    }

    m_layoutUpdateRequired = false;
}


void QnVideowallScreenWidget::at_thumbnailReady(const QnUuid &resourceId, const QPixmap &thumbnail) {
    m_thumbs[resourceId] = thumbnail;
    update();
}

void QnVideowallScreenWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item) {
    Q_UNUSED(videoWall)

    int idx = qnIndexOf(m_items, [&item](const QnVideoWallItem &i) {return item.uuid == i.uuid; });
    if (idx < 0)
        return; // item on another widget

    QnVideoWallItem oldItem = m_items[idx];
    if (oldItem.screenSnaps.screens() != item.screenSnaps.screens()) {
         // if there are more than one item on the widget, this one will be updated from outside
        if (m_items.size() == 1)
            updateItems(); 
        return; 
    }
    
    m_items[idx] = item;
    m_layoutUpdateRequired = (item.screenSnaps != oldItem.screenSnaps);
    update();
}

void QnVideowallScreenWidget::at_itemDataChanged(int role) {
    base_type::at_itemDataChanged(role);
    if (role != Qn::VideoWallItemIndicesRole)
        return;

    updateItems();
}

void QnVideowallScreenWidget::updateItems() {
    m_items.clear();

    QnVideoWallItemIndexList indices = item()->data(Qn::VideoWallItemIndicesRole).value<QnVideoWallItemIndexList>();
    foreach (const QnVideoWallItemIndex &index, indices) {
        if (!index.isValid())
            continue;
        m_items << index.item();
    }

    if (!m_items.isEmpty()) {
        QnVideoWallPcData pc = m_videowall->pcs()->getItem(m_items.first().pcUuid);
        
        QRect totalDesktopGeometry;
        QSet<int> screens = m_items.first().screenSnaps.screens();
        foreach (const QnVideoWallPcData::PcScreen &screen, pc.screens) {
            if (!screens.contains(screen.index))
                continue;
            totalDesktopGeometry = totalDesktopGeometry.united(screen.desktopGeometry);
        }
        if (totalDesktopGeometry.isValid())
            setAspectRatio((qreal)totalDesktopGeometry.width() / totalDesktopGeometry.height());
    }

    updateLayout(true);
    updateTitleText();
    update();
}
