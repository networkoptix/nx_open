// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_screen_widget.h"

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsAnchorLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <core/resource/layout_item_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/videowall/utils.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/resource/videowall_item_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

using namespace nx::vms::client::desktop;

namespace {

static const qreal kMargin = 3.0;
static const qreal kTopMargin = 20.0;

} // namespace

QnVideowallScreenWidget::QnVideowallScreenWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    m_videowall(base_type::resource().dynamicCast<QnVideoWallResource>()),
    m_mainOverlayWidget(new QnViewportBoundWidget(this)),
    m_layout(new QGraphicsAnchorLayout())
{
    NX_ASSERT(m_videowall, "QnVideowallScreenWidget was created with a non-videowall resource.");

    setAcceptDrops(true);
    setOption(QnResourceWidget::AlwaysShowName);
    setOption(QnResourceWidget::WindowRotationForbidden);
    setPaletteColor(this, QPalette::Window, nx::vms::client::core::colorTheme()->color("dark4"));

    m_layout->setContentsMargins(kMargin, kTopMargin, kMargin, kMargin);
    m_layout->setSpacing(0.0);

    m_mainOverlayWidget->setLayout(m_layout);
    m_mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    m_mainOverlayWidget->setOpacity(1.0);
    addOverlayWidget(m_mainOverlayWidget, {UserVisible, OverlayFlag::autoRotate});

    updateItems();
    updateButtonsVisibility();
    updateTitleText();

    connect(m_videowall.get(), &QnVideoWallResource::itemChanged, this,
        &QnVideowallScreenWidget::at_videoWall_itemChanged);
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

    auto screens = screensCoveredByItem(m_items.first(), m_videowall).values();
    std::sort(screens.begin(), screens.end());
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
        .arg(screenIndices.join(lit(", ")));
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

void QnVideowallScreenWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
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

    if (screensCoveredByItem(*existing, videoWall) != screensCoveredByItem(item, videoWall))
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

void QnVideowallScreenWidget::atItemDataChanged(Qn::ItemDataRole role)
{
    base_type::atItemDataChanged(role);
    if (role != Qn::VideoWallItemIndicesRole)
        return;

    updateItems();
}

int QnVideowallScreenWidget::helpTopicAt(const QPointF& /*pos*/) const
{
    return HelpTopic::Id::Videowall_Display;
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
        QSet<int> screens = screensCoveredByItem(m_items.first(), m_videowall);
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
