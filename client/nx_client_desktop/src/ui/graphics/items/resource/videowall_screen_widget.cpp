#include "videowall_screen_widget.h"

#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsAnchorLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <nx/client/desktop/image_providers/camera_thumbnail_manager.h>

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
#include <ui/workbench/workbench.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <ui/style/skin.h>

#include <utils/common/warnings.h>
#include <nx/utils/collection.h>

namespace {

static const qreal kMargin = 3.0;
static const qreal kTopMargin = 20.0;

} // namespace

QnVideowallScreenWidget::QnVideowallScreenWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent),
    m_videowall(base_type::resource().dynamicCast<QnVideoWallResource>()),
    m_mainOverlayWidget(new QnViewportBoundWidget(this)),
    m_layout(new QGraphicsAnchorLayout()),
    m_thumbnailManager(context->instance<QnCameraThumbnailManager>())
{
    NX_ASSERT(m_videowall, "QnVideowallScreenWidget was created with a non-videowall resource.");
    m_thumbnailManager->setAutoRotate(false); //< TODO: VMS-6759

    setAcceptDrops(true);
    setOption(QnResourceWidget::WindowRotationForbidden, true);

    m_layout->setContentsMargins(kMargin, kTopMargin, kMargin, kMargin);
    m_layout->setSpacing(0.0);

    m_mainOverlayWidget->setLayout(m_layout);
    m_mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    m_mainOverlayWidget->setOpacity(1.0);
    addOverlayWidget(m_mainOverlayWidget, detail::OverlayParams(UserVisible, true));

    updateItems();
    updateButtonsVisibility();
    updateTitleText();
    setInfoVisible(true, false);
    updateInfoText();

    connect(m_videowall, &QnVideoWallResource::itemChanged, this,
        &QnVideowallScreenWidget::at_videoWall_itemChanged);

    connect(m_thumbnailManager, &QnCameraThumbnailManager::thumbnailReady, this,
        &QnVideowallScreenWidget::at_thumbnailReady);

}

QnVideowallScreenWidget::~QnVideowallScreenWidget()
{
}

QnVideoWallResourcePtr QnVideowallScreenWidget::videowall() const
{
    return m_videowall;
}

Qn::RenderStatus QnVideowallScreenWidget::paintChannelBackground(QPainter* painter,
    int /*channel*/, const QRectF& /*channelRect*/, const QRectF& paintRect)
{
    if (!paintRect.isValid())
        return Qn::NothingRendered;

    updateLayout();
    painter->fillRect(paintRect, palette().color(QPalette::Mid));
    return Qn::NewFrameRendered;
}

int QnVideowallScreenWidget::calculateButtonsVisibility() const
{
    return 0;
}

QString QnVideowallScreenWidget::calculateTitleText() const
{
    if (m_items.isEmpty())
        return QString();

    auto pcUuid = m_videowall->pcs()->getItem(m_items.first().pcUuid).uuid;

    auto pcUuids = m_videowall->pcs()->getItems().keys();
    std::sort(pcUuids.begin(), pcUuids.end());
    int idx = pcUuids.indexOf(pcUuid);
    if (idx < 0)
        idx = 0;

    int pcVisualIdx = idx + 1;
    QString base = tr("PC %1").arg(pcVisualIdx);

    QSet<int> screens = m_items.first().screenSnaps.screens();
    if (screens.isEmpty())
        return base;

    QStringList screenIndices;
    for (int screen: screens)
        screenIndices.append(QString::number(screen + 1));

    if (screenIndices.size() == 1)
        return tr("PC %1 - Display %2").arg(pcVisualIdx).arg(screenIndices.first());

    return tr("PC %1 - Displays %2",
        "%2 will be substituted by _list_ of displays",
        screens.size())
        .arg(pcVisualIdx)
        .arg(screenIndices.join(lit(" ,")));
}

Qn::ResourceStatusOverlay QnVideowallScreenWidget::calculateStatusOverlay() const
{
    if (renderStatus() == Qn::NothingRendered)
        return Qn::LoadingOverlay;

    return Qn::EmptyOverlay;
}

void QnVideowallScreenWidget::updateLayout(bool force)
{
    if (!m_layoutUpdateRequired && !force)
        return;

    while (m_layout->count() > 0)
    {
        auto item = m_layout->itemAt(0);
        m_layout->removeAt(0);
        delete item;
    }

    auto createItem = [this](const QnUuid &id)
        {
            auto itemWidget = new QnVideowallItemWidget(m_videowall, id, this, m_mainOverlayWidget);
            connect(itemWidget, &QnClickableWidget::clicked, this,
                [this](Qt::MouseButton button)
                {
                    if (button != Qt::LeftButton)
                        return;
                    workbench()->setItem(Qn::SingleSelectedRole, item());
                });

            return itemWidget;
        };


    auto partOfScreen = [](const QnScreenSnaps &snaps)
        {
            return std::any_of(snaps.values.cbegin(), snaps.values.cend(),
                [](const QnScreenSnap &snap)
                {
                    return snap.snapIndex > 0;
                });
        };

    // can have several items on a single screen - or one item can take just part of the screen
    if (m_items.size() > 1 || (m_items.size() == 1 && partOfScreen(m_items.first().screenSnaps)))
    {
        for (const auto& item: m_items)
        {
            auto itemWidget = createItem(item.uuid);
            NX_ASSERT(QnScreenSnap::snapsPerScreen() == 2);    //in other case layout should be reimplemented

            if (item.screenSnaps.left().snapIndex == 0)
                m_layout->addAnchor(itemWidget, Qt::AnchorLeft, m_layout, Qt::AnchorLeft);
            else
                m_layout->addAnchor(itemWidget, Qt::AnchorLeft, m_layout, Qt::AnchorHorizontalCenter);

            if (item.screenSnaps.top().snapIndex == 0)
                m_layout->addAnchor(itemWidget, Qt::AnchorTop, m_layout, Qt::AnchorTop);
            else
                m_layout->addAnchor(itemWidget, Qt::AnchorTop, m_layout, Qt::AnchorVerticalCenter);

            if (item.screenSnaps.right().snapIndex == 0)
                m_layout->addAnchor(itemWidget, Qt::AnchorRight, m_layout, Qt::AnchorRight);
            else
                m_layout->addAnchor(itemWidget, Qt::AnchorRight, m_layout, Qt::AnchorHorizontalCenter);

            if (item.screenSnaps.bottom().snapIndex == 0)
                m_layout->addAnchor(itemWidget, Qt::AnchorBottom, m_layout, Qt::AnchorBottom);
            else
                m_layout->addAnchor(itemWidget, Qt::AnchorBottom, m_layout, Qt::AnchorVerticalCenter);

        }
    }
    else if (m_items.size() == 1)
    {    // can have only one item on several screens
        auto itemWidget = createItem(m_items.first().uuid);
        m_layout->addAnchors(itemWidget, m_layout, Qt::Horizontal | Qt::Vertical);
    }

    m_layoutUpdateRequired = false;
}


void QnVideowallScreenWidget::at_thumbnailReady(const QnUuid& resourceId, const QPixmap& thumbnail)
{
    m_thumbs[resourceId] = thumbnail;
    update();
}

void QnVideowallScreenWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr& /*videoWall*/,
    const QnVideoWallItem& item,
    const QnVideoWallItem& oldItem)
{
    auto existing = std::find_if(m_items.begin(), m_items.end(),
        [&item](const QnVideoWallItem& i)
        {
            return item.uuid == i.uuid;
        });

    // Check if item is on another widget
    if (existing == m_items.end())
        return;

    // Check if item is already updated from another call.
    if (*existing == item)
        return;

    NX_ASSERT(*existing == oldItem);

    if (existing->screenSnaps.screens() != item.screenSnaps.screens())
    {
        // if there are more than one item on the widget, this one will be updated from outside
        if (m_items.size() == 1)
            updateItems();

        return;
    }

    m_layoutUpdateRequired |= (item.screenSnaps != existing->screenSnaps);
    *existing = item;
    update();
}

void QnVideowallScreenWidget::at_itemDataChanged(int role)
{
    base_type::at_itemDataChanged(role);
    if (role != Qn::VideoWallItemIndicesRole)
        return;

    updateItems();
}

int QnVideowallScreenWidget::helpTopicAt(const QPointF& /*pos*/) const
{
    return Qn::Videowall_Display_Help;
}

void QnVideowallScreenWidget::updateItems()
{
    m_items.clear();

    QnVideoWallItemIndexList indices = item()->data(Qn::VideoWallItemIndicesRole)
        .value<QnVideoWallItemIndexList>();

    for (const QnVideoWallItemIndex& index: indices)
    {
        if (index.isValid())
            m_items << index.item();
    }

    if (!m_items.isEmpty())
    {
        QnVideoWallPcData pc = m_videowall->pcs()->getItem(m_items.first().pcUuid);

        QRect totalDesktopGeometry;
        QSet<int> screens = m_items.first().screenSnaps.screens();
        for (const auto& screen: pc.screens)
        {
            if (screens.contains(screen.index))
                totalDesktopGeometry = totalDesktopGeometry.united(screen.desktopGeometry);
        }

        if (totalDesktopGeometry.isValid())
            setAspectRatio((qreal)totalDesktopGeometry.width() / totalDesktopGeometry.height());
    }

    updateLayout(true);
    updateTitleText();
    update();
}
