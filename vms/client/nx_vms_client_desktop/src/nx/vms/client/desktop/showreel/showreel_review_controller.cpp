// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_review_controller.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>

#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/cpp14.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/core/utils/grid_walker.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_utility.h>
#include <utils/common/delayed.h>

#include "showreel_drop_placeholder.h"
#include "showreel_state_manager.h"

namespace {

static constexpr int kDefaultDelayMs = 5000;

// Save showreel no more often than once in 5 seconds.
static constexpr int kSaveShowreelIntervalMs = 5000;

// Update items layout no more often than once in a second.
static constexpr int kUpdateItemsLayoutIntervalMs = 1000;

static const QSize kCellSize{1, 1};

static const QMargins kReviewMargins(16, 16, 16, 16);

// Placeholders are allowed only for grids less or equal than 4*4.
static const int kMaxItemCountWithPlaceholders = 15;

// Grid size less than 2*2 is not allowed.
static const int kMinGridSize = 2;

QRect createItemGrid(int itemCount)
{
    const bool addPlaceholder = itemCount <= kMaxItemCountWithPlaceholders;
    if (addPlaceholder)
        ++itemCount;

    int h = std::max((int) std::ceil(std::sqrt(itemCount)), kMinGridSize);
    int w = std::max((int) std::ceil(1.0 * itemCount / h), kMinGridSize);
    return QRect(0, 0, w, h);
}

} // namespace

namespace nx::vms::client::desktop {

ShowreelReviewController::ShowreelReviewController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    auto saveQueuedShowreels =
        [this]
        {
            for (const auto& id: m_saveShowreelsQueue)
                menu()->trigger(menu::SaveShowreelAction, {Qn::UuidRole, id});
            m_saveShowreelsQueue.clear();
        };
    m_saveShowreelsOperation =
        new nx::utils::PendingOperation(saveQueuedShowreels, kSaveShowreelIntervalMs, this);

    auto updateItemsLayout = [this] { this->updateItemsLayout(); };
    m_updateItemsLayoutOperation =
        new nx::utils::PendingOperation(updateItemsLayout, kUpdateItemsLayoutIntervalMs, this);

    connect(systemContext()->showreelManager(), &common::ShowreelManager::showreelChanged, this,
        &ShowreelReviewController::handleShowreelChanged);

    connect(systemContext()->showreelManager(), &common::ShowreelManager::showreelRemoved, this,
        &ShowreelReviewController::handleShowreelRemoved);

    connect(action(menu::ReviewShowreelAction), &QAction::triggered, this,
        &ShowreelReviewController::at_reviewShowreelAction_triggered);

    connect(action(menu::DropResourcesAction), &QAction::triggered, this,
        &ShowreelReviewController::at_dropResourcesAction_triggered);

    connect(action(menu::StartCurrentShowreelAction), &QAction::triggered, this,
        &ShowreelReviewController::at_startCurrentShowreelAction_triggered);

    connect(action(menu::SaveCurrentShowreelAction), &QAction::triggered, this,
        &ShowreelReviewController::at_saveCurrentShowreelAction_triggered);

    connect(action(menu::RemoveCurrentShowreelAction), &QAction::triggered, this,
        &ShowreelReviewController::at_removeCurrentShowreelAction_triggered);

    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        &ShowreelReviewController::stopListeningLayout);

    connect(workbench(), &Workbench::currentLayoutChanged, this,
        &ShowreelReviewController::startListeningLayout);
}

ShowreelReviewController::~ShowreelReviewController()
{
    stopListeningLayout();
}

void ShowreelReviewController::handleShowreelChanged(const nx::vms::api::ShowreelData& showreel)
{
    // Handle only Showreels we are currently reviewing.
    auto reviewLayout = m_reviewLayouts.value(showreel.id);
    if (!reviewLayout)
        return;

    reviewLayout->setName(showreel.name);
    reviewLayout->setData(Qn::ShowreelIsManualRole, showreel.settings.manual);
    auto wbLayout = workbench()->layout(reviewLayout);
    if (wbLayout)
        wbLayout->setData(Qn::ShowreelIsManualRole, showreel.settings.manual);

    // If layout is not current, simply overwrite it.
    if (workbench()->currentLayout() != wbLayout)
    {
        resetReviewLayout(reviewLayout, showreel.items);
        return;
    }
    m_updateItemsLayoutOperation->requestOperation();
    updateButtons(reviewLayout);
}

void ShowreelReviewController::handleShowreelRemoved(const QnUuid& showreelId)
{
    m_saveShowreelsQueue.remove(showreelId);

    if (currentShowreelId() == showreelId)
        stopListeningLayout();

    // Handle only Showreels we are currently reviewing.
    if (auto reviewLayout = m_reviewLayouts.take(showreelId))
    {
        workbench()->removeLayout(reviewLayout);
        if (workbench()->layouts().empty())
            menu()->trigger(menu::OpenNewTabAction);
    }
}

void ShowreelReviewController::startListeningLayout()
{
    if (!isShowreelReviewMode())
        return;

    NX_ASSERT(m_reviewLayouts.values().contains(workbench()->currentLayoutResource()));
    m_connections = {};
    connectToLayout(workbench()->currentLayout());
    updateOrder();
    updateButtons(workbench()->currentLayoutResource());
    updatePlaceholders();
}

void ShowreelReviewController::stopListeningLayout()
{
    m_connections = {};
    m_dropPlaceholders.clear();
}

void ShowreelReviewController::reviewShowreel(const nx::vms::api::ShowreelData& showreel)
{
    if (!showreel.isValid())
        return;

    if (auto layout = m_reviewLayouts.value(showreel.id))
    {
        menu()->trigger(menu::OpenInNewTabAction, layout);
        updateItemsLayout();
        return;
    }

    const QList<menu::IDType> actions{menu::StartCurrentShowreelAction};

    static const float kCellAspectRatio{16.0f / 9.0f};

    const auto layout = LayoutResourcePtr(new LayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid()); //< Layout is never saved to server.
    layout->addFlags(Qn::local);
    layout->setParentId(showreel.id);
    layout->setName(showreel.name);
    layout->setData(Qn::IsSpecialLayoutRole, true);
    layout->setData(Qn::LayoutIconRole, qnResIconCache->icon(QnResourceIconCache::Showreel));
    layout->setData(Qn::LayoutFlagsRole, QVariant::fromValue(QnLayoutFlag::FixedViewport
        | QnLayoutFlag::NoResize
        | QnLayoutFlag::NoTimeline
        | QnLayoutFlag::FillViewport
    ));
    layout->setData(Qn::LayoutMarginsRole, QVariant::fromValue(kReviewMargins));
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::ShowreelUuidRole, QVariant::fromValue(showreel.id));
    layout->setData(Qn::ShowreelIsManualRole, showreel.settings.manual);
    layout->setCellAspectRatio(kCellAspectRatio);
    m_reviewLayouts.insert(showreel.id, layout);

    resetReviewLayout(layout, showreel.items);

    menu()->trigger(menu::OpenInNewTabAction, layout);
}

QnUuid ShowreelReviewController::currentShowreelId() const
{
    return workbench()->currentLayout()->data(Qn::ShowreelUuidRole).value<QnUuid>();
}

bool ShowreelReviewController::isShowreelReviewMode() const
{
    return !currentShowreelId().isNull();
}

void ShowreelReviewController::connectToLayout(QnWorkbenchLayout* layout)
{
    if (!NX_ASSERT(layout->resource()))
       return;

    auto saveAndUpdateLayout = [this]
        {
            if (!isShowreelReviewMode())
                return;

            // Saving must go before layout updating to make sure item order will be correct.
            menu()->trigger(menu::SaveCurrentShowreelAction);
            m_updateItemsLayoutOperation->requestOperation();
        };

    // Queued connection to call save after batch operation is complete.
    m_connections << connect(layout, &QnWorkbenchLayout::itemAdded, this, saveAndUpdateLayout,
        Qt::QueuedConnection);
    m_connections << connect(layout, &QnWorkbenchLayout::itemsMoved, this, saveAndUpdateLayout,
        Qt::QueuedConnection);
    m_connections << connect(layout, &QnWorkbenchLayout::itemRemoved, this, saveAndUpdateLayout,
        Qt::QueuedConnection);

    m_connections << connect(layout->resource().get(), &LayoutResource::itemDataChanged, this,
        &ShowreelReviewController::handleItemDataChanged);
}

void ShowreelReviewController::updateOrder()
{
    NX_ASSERT_HEAVY_CONDITION(isShowreelReviewMode());

    auto items = workbench()->currentLayout()->items().values();
    QnWorkbenchItem::sortByGeometryAndName(items);

    int index = 0;
    for (auto item: items)
        item->setData(Qn::ShowreelItemOrderRole, ++index);
}

void ShowreelReviewController::updateButtons(const LayoutResourcePtr& layout)
{
    NX_ASSERT(layout);
    if (!layout)
        return;

    // Using he fact that dataChanged will be sent even if action list was not changed.
    const QList<menu::IDType> actions{menu::StartCurrentShowreelAction};
    layout->setData(Qn::CustomPanelActionsRole, QVariant::fromValue(actions));
}

void ShowreelReviewController::updatePlaceholders()
{
    auto createPlaceholder =
        [this, mapper = workbench()->mapper()](const QPoint& cell)
        {
            const QRectF geometry = mapper->mapFromGrid({cell, kCellSize});

            QSharedPointer<ShowreelDropPlaceholder> result(
                new ShowreelDropPlaceholder());
            result->setRect(geometry);
            display()->setLayer(result.data(), QnWorkbenchDisplay::BackLayer);
            mainWindow()->scene()->addItem(result.data());
            return result;
        };

    const auto layout = workbench()->currentLayout();
    if (!NX_ASSERT(layout->resource()))
    {
        m_dropPlaceholders.clear();
        return;
    }

    const int itemCount = layout->items().size();
    if (itemCount > kMaxItemCountWithPlaceholders)
    {
        m_dropPlaceholders.clear();
        return;
    }

    QRect boundingRect = createItemGrid(itemCount);

    // Copy list to avoid crash while iterating.
    const auto existingPlaceholders = m_dropPlaceholders.keys();
    for (const auto& p: existingPlaceholders)
    {
        if (!boundingRect.contains(p))
            m_dropPlaceholders.remove(p);
    }

    int placeholdersCount = 0;
    for (int x = boundingRect.left(); x <= boundingRect.right(); ++x)
    {
        for (int y = boundingRect.top(); y <= boundingRect.bottom(); ++y)
        {
            const QPoint cell(x, y);
            const bool isFree = layout->isFreeSlot(cell, kCellSize);
            const bool placeholderExists = m_dropPlaceholders.contains(cell);
            const bool mustBePlaceholder = isFree;

            // If cell is empty and there is a placeholder (or vise versa), skip this step.
            if (mustBePlaceholder == placeholderExists)
            {
                if (placeholderExists)
                    ++placeholdersCount;
                continue;
            }

            if (placeholderExists)
            {
                m_dropPlaceholders.remove(cell);
            }
            else
            {
                m_dropPlaceholders.insert(cell, createPlaceholder(cell));
                ++placeholdersCount;
            }
        }
    }

    layout->setData(Qn::LayoutMinimalBoundingRectRole, boundingRect);
    const bool animate = display()->animationAllowed() && layout->items().size() > 0;
    display()->fitInView(animate);
}

void ShowreelReviewController::updateItemsLayout()
{
    if (!isShowreelReviewMode())
        return;

    const auto wbLayout = workbench()->currentLayout();
    const auto showreelId = currentShowreelId();
    const auto showreel = systemContext()->showreelManager()->showreel(currentShowreelId());
    const auto reviewLayout = wbLayout->resource();
    NX_ASSERT(reviewLayout == m_reviewLayouts.value(showreelId));

    nx::vms::api::ShowreelDataList currentItems;

    auto layoutItems = wbLayout->items().values();
    QnWorkbenchItem::sortByGeometryAndName(layoutItems);

    for (auto layoutItem: layoutItems)
    {
        const bool unpinned = wbLayout->unpinItem(layoutItem);
        NX_ASSERT(unpinned);
    }

    const auto itemGrid = createItemGrid((int)showreel.items.size());
    core::GridWalker walker(itemGrid);


    // Dynamically reorder existing items to match new order (if something was added or removed).
    for (const auto& item: showreel.items)
    {
        // Find first existing item that match the resource.
        auto existing = std::find_if(layoutItems.begin(), layoutItems.end(),
            [item](QnWorkbenchItem* existing)
            {
                const auto resource = existing->resource();
                return resource && resource->getId() == item.resourceId;
            });

        const bool hasPosition = walker.next();
        NX_ASSERT(hasPosition);
        // Move existing item to the selected place.
        if (existing != layoutItems.end())
        {
            auto layoutItem = *existing;
            const bool pinned = wbLayout->pinItem(layoutItem, {walker.pos(), kCellSize});
            NX_ASSERT(pinned);
            reviewLayout->setItemData(
                layoutItem->uuid(),
                Qn::ShowreelItemDelayMsRole,
                item.delayMs);
            layoutItems.erase(existing);
        }
        else
        {
            addItemToReviewLayout(reviewLayout, item, walker.pos(), true);
        }
    }

    // These are items that were not repositioned.
    for (auto layoutItem: layoutItems)
    {
        reviewLayout->cleanupItemData(layoutItem->uuid());
        reviewLayout->removeItem(layoutItem->uuid());
    }

    updatePlaceholders();
    updateOrder();
}

void ShowreelReviewController::resetReviewLayout(
    const LayoutResourcePtr& layout,
    const nx::vms::api::ShowreelItemDataList& items)
{
    layout->cleanupItemData();
    layout->setItems(common::LayoutItemDataList());

    const int gridSize = std::min((int)items.size(), qnRuntime->maxSceneItems());

    const auto grid = createItemGrid(gridSize);
    core::GridWalker walker(grid);

    for (const auto& item: items)
    {
        const bool hasPosition = walker.next();
        NX_ASSERT(hasPosition);

        addItemToReviewLayout(layout, item, walker.pos(), true);
    }
    updateButtons(layout);
}

void ShowreelReviewController::addItemToReviewLayout(
    const LayoutResourcePtr& layout,
    const nx::vms::api::ShowreelItemData& item,
    const QPointF& position,
    bool pinItem)
{
    if (layout->getItems().size() >= qnRuntime->maxSceneItems())
        return;

    common::LayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.combinedGeometry = QRectF(position, kCellSize);
    if (pinItem)
        itemData.flags = Qn::Pinned;
    itemData.resource.id = item.resourceId;
    layout->setItemData(itemData.uuid,
        Qn::ShowreelItemDelayMsRole,
        item.delayMs);
    layout->addItem(itemData);
}

void ShowreelReviewController::addResourcesToReviewLayout(
    const LayoutResourcePtr& layout,
    const QnResourceList& resources,
    const QPointF& position)
{
    NX_ASSERT(!m_updating);
    if (m_updating)
        return;

    const auto resourceCanBeAddedToShowreel =
        [](const QnResourcePtr& resource)
        {
            if (const auto layout = resource.dynamicCast<QnLayoutResource>())
                return !layout::isEncrypted(layout);

            return QnResourceAccessFilter::isOpenableInLayout(resource);
        };

    QScopedValueRollback<bool> guard(m_updating, true);
    for (const auto& resource: resources.filtered(resourceCanBeAddedToShowreel))
        addItemToReviewLayout(layout, {resource->getId(), kDefaultDelayMs}, position, false);
}

bool ShowreelReviewController::fillShowreelItems(nx::vms::api::ShowreelItemDataList* items)
{
    NX_ASSERT(items);
    if (!items)
        return false;

    auto layoutItems = workbench()->currentLayout()->items().values();
    QnWorkbenchItem::sortByGeometryAndName(layoutItems);
    for (auto item: layoutItems)
    {
        const auto resource = item->resource();
        NX_ASSERT(resource);
        if (!resource)
            continue;

        const auto delayMs = item->data(Qn::ShowreelItemDelayMsRole).toInt();
        items->emplace_back(resource->getId(), delayMs);
    }

    return true;
}

void ShowreelReviewController::handleItemDataChanged(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    if (role != Qn::ShowreelItemDelayMsRole)
        return;

    // We can get here while changing layout.
    if (!isShowreelReviewMode())
        return;

    const auto item = workbench()->currentLayout()->item(id);
    if (!item)
        return;

    QSet<QnUuid> selectedItems;
    for (const auto widget: display()->widgets())
    {
        if (widget->isSelected())
            selectedItems.insert(widget->item()->uuid());
    }

    if (!selectedItems.contains(id))
        return;

    auto layout = workbench()->currentLayoutResource();

    selectedItems.remove(id);
    for (const auto& itemId: selectedItems)
        layout->setItemData(itemId, role, data);
}

void ShowreelReviewController::at_reviewShowreelAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto id = parameters.argument<QnUuid>(Qn::UuidRole);
    reviewShowreel(systemContext()->showreelManager()->showreel(id));
}

void ShowreelReviewController::at_dropResourcesAction_triggered()
{
    if (!isShowreelReviewMode())
        return;

    const auto reviewLayout = workbench()->currentLayoutResource();
    const auto parameters = menu()->currentParameters(sender());
    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);
    addResourcesToReviewLayout(reviewLayout, parameters.resources(), position);
    const auto showreel = systemContext()->showreelManager()->showreel(currentShowreelId());
    menu()->trigger(menu::SaveCurrentShowreelAction);
}

void ShowreelReviewController::at_startCurrentShowreelAction_triggered()
{
    NX_ASSERT_HEAVY_CONDITION(isShowreelReviewMode());
    const auto showreel = systemContext()->showreelManager()->showreel(currentShowreelId());
    NX_ASSERT(showreel.isValid());
    const auto startTourAction = action(menu::ToggleShowreelModeAction);
    NX_ASSERT(!startTourAction->isChecked());
    if (!startTourAction->isChecked())
        menu()->trigger(menu::ToggleShowreelModeAction, {Qn::UuidRole, showreel.id});
}

void ShowreelReviewController::at_saveCurrentShowreelAction_triggered()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    NX_ASSERT_HEAVY_CONDITION(isShowreelReviewMode());
    const auto id = currentShowreelId();
    auto showreel = systemContext()->showreelManager()->showreel(id);
    NX_ASSERT(showreel.isValid());

    const auto reviewLayout = m_reviewLayouts.value(id);
    NX_ASSERT(reviewLayout);

    showreel.items.clear();
    fillShowreelItems(&showreel.items);
    showreel.settings.manual =
        workbench()->currentLayout()->data(Qn::ShowreelIsManualRole).toBool();
    systemContext()->showreelManager()->addOrUpdateShowreel(showreel);
    systemContext()->showreelStateManager()->markChanged(showreel.id, true);

    m_saveShowreelsQueue.insert(showreel.id);
    m_saveShowreelsOperation->requestOperation();
}

void ShowreelReviewController::at_removeCurrentShowreelAction_triggered()
{
    NX_ASSERT_HEAVY_CONDITION(isShowreelReviewMode());
    menu()->trigger(menu::RemoveShowreelAction, {Qn::UuidRole, currentShowreelId()});
}

} // namespace nx::vms::client::desktop
