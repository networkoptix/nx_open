// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_tour_review_controller.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/std/cpp14.h>
#include <nx/vms/client/core/utils/grid_walker.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/graphics/items/resource/layout_tour_drop_placeholder.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_utility.h>
#include <utils/common/delayed.h>

namespace {

static constexpr int kDefaultDelayMs = 5000;

// Save layout tour no more often than once in 5 seconds.
static constexpr int kSaveTourIntervalMs = 5000;

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
namespace ui {
namespace workbench {

LayoutTourReviewController::LayoutTourReviewController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    auto saveQueuedTours = [this]
        {
            for (const auto& id: m_saveToursQueue)
                menu()->trigger(action::SaveLayoutTourAction, {Qn::UuidRole, id});
            m_saveToursQueue.clear();
        };
    m_saveToursOperation =
        new nx::utils::PendingOperation(saveQueuedTours, kSaveTourIntervalMs, this);

    auto updateItemsLayout = [this]
        {
            this->updateItemsLayout();
        };
    m_updateItemsLayoutOperation =
        new nx::utils::PendingOperation(updateItemsLayout, kUpdateItemsLayoutIntervalMs, this);

    connect(layoutTourManager(), &QnLayoutTourManager::tourChanged, this,
        &LayoutTourReviewController::handleTourChanged);

    connect(layoutTourManager(), &QnLayoutTourManager::tourRemoved, this,
        &LayoutTourReviewController::handleTourRemoved);

    connect(action(action::ReviewLayoutTourAction), &QAction::triggered, this,
        &LayoutTourReviewController::at_reviewLayoutTourAction_triggered);

    connect(action(action::DropResourcesAction), &QAction::triggered, this,
        &LayoutTourReviewController::at_dropResourcesAction_triggered);

    connect(action(action::StartCurrentLayoutTourAction), &QAction::triggered, this,
        &LayoutTourReviewController::at_startCurrentLayoutTourAction_triggered);

    connect(action(action::SaveCurrentLayoutTourAction), &QAction::triggered, this,
        &LayoutTourReviewController::at_saveCurrentLayoutTourAction_triggered);

    connect(action(action::RemoveCurrentLayoutTourAction), &QAction::triggered, this,
        &LayoutTourReviewController::at_removeCurrentLayoutTourAction_triggered);

    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        &LayoutTourReviewController::stopListeningLayout);

    connect(workbench(), &Workbench::currentLayoutChanged, this,
        &LayoutTourReviewController::startListeningLayout);
}

LayoutTourReviewController::~LayoutTourReviewController()
{
    stopListeningLayout();
}

void LayoutTourReviewController::handleTourChanged(const nx::vms::api::LayoutTourData& tour)
{
    // Handle only tours we are currently reviewing.
    auto reviewLayout = m_reviewLayouts.value(tour.id);
    if (!reviewLayout)
        return;

    reviewLayout->setName(tour.name);
    reviewLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    auto wbLayout = workbench()->layout(reviewLayout);
    if (wbLayout)
        wbLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);

    // If layout is not current, simply overwrite it.
    if (workbench()->currentLayout() != wbLayout)
    {
        resetReviewLayout(reviewLayout, tour.items);
        return;
    }
    m_updateItemsLayoutOperation->requestOperation();
    updateButtons(reviewLayout);
}

void LayoutTourReviewController::handleTourRemoved(const QnUuid& tourId)
{
    m_saveToursQueue.remove(tourId);

    if (currentTourId() == tourId)
        stopListeningLayout();

    // Handle only tours we are currently reviewing.
    if (auto reviewLayout = m_reviewLayouts.take(tourId))
    {
        workbench()->removeLayout(reviewLayout);
        if (workbench()->layouts().empty())
            menu()->trigger(action::OpenNewTabAction);
    }
}

void LayoutTourReviewController::startListeningLayout()
{
    if (!isLayoutTourReviewMode())
        return;

    NX_ASSERT(m_reviewLayouts.values().contains(workbench()->currentLayoutResource()));
    m_connections = {};
    connectToLayout(workbench()->currentLayout());
    updateOrder();
    updateButtons(workbench()->currentLayoutResource());
    updatePlaceholders();
}

void LayoutTourReviewController::stopListeningLayout()
{
    m_connections = {};
    m_dropPlaceholders.clear();
}

void LayoutTourReviewController::reviewLayoutTour(const nx::vms::api::LayoutTourData& tour)
{
    if (!tour.isValid())
        return;

    if (auto layout = m_reviewLayouts.value(tour.id))
    {
        menu()->trigger(action::OpenInNewTabAction, layout);
        updateItemsLayout();
        return;
    }

    const QList<action::IDType> actions{action::StartCurrentLayoutTourAction};

    static const float kCellAspectRatio{16.0f / 9.0f};

    const auto layout = LayoutResourcePtr(new LayoutResource());
    layout->setIdUnsafe(QnUuid::createUuid()); //< Layout is never saved to server.
    layout->addFlags(Qn::local);
    layout->setParentId(tour.id);
    layout->setName(tour.name);
    layout->setData(Qn::IsSpecialLayoutRole, true);
    layout->setData(Qn::LayoutIconRole, qnResIconCache->icon(QnResourceIconCache::LayoutTour));
    layout->setData(Qn::LayoutFlagsRole, QVariant::fromValue(QnLayoutFlag::FixedViewport
        | QnLayoutFlag::NoResize
        | QnLayoutFlag::NoTimeline
        | QnLayoutFlag::FillViewport
    ));
    layout->setData(Qn::LayoutMarginsRole, QVariant::fromValue(kReviewMargins));
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::LayoutTourUuidRole, QVariant::fromValue(tour.id));
    layout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    layout->setCellAspectRatio(kCellAspectRatio);
    m_reviewLayouts.insert(tour.id, layout);

    resetReviewLayout(layout, tour.items);

    menu()->trigger(action::OpenInNewTabAction, layout);
}

QnUuid LayoutTourReviewController::currentTourId() const
{
    return workbench()->currentLayout()->data(Qn::LayoutTourUuidRole).value<QnUuid>();
}

bool LayoutTourReviewController::isLayoutTourReviewMode() const
{
    return !currentTourId().isNull();
}

void LayoutTourReviewController::connectToLayout(QnWorkbenchLayout* layout)
{
    if (!NX_ASSERT(layout->resource()))
       return;

    auto saveAndUpdateLayout = [this]
        {
            if (!isLayoutTourReviewMode())
                return;

            // Saving must go before layout updating to make sure item order will be correct.
            menu()->trigger(action::SaveCurrentLayoutTourAction);
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
        &LayoutTourReviewController::handleItemDataChanged);
}

void LayoutTourReviewController::updateOrder()
{
    NX_ASSERT_HEAVY_CONDITION(isLayoutTourReviewMode());

    auto items = workbench()->currentLayout()->items().values();
    QnWorkbenchItem::sortByGeometryAndName(items);

    int index = 0;
    for (auto item: items)
        item->setData(Qn::LayoutTourItemOrderRole, ++index);
}

void LayoutTourReviewController::updateButtons(const LayoutResourcePtr& layout)
{
    NX_ASSERT(layout);
    if (!layout)
        return;

    // Using he fact that dataChanged will be sent even if action list was not changed.
    const QList<action::IDType> actions{action::StartCurrentLayoutTourAction};
    layout->setData(Qn::CustomPanelActionsRole, QVariant::fromValue(actions));
}

void LayoutTourReviewController::updatePlaceholders()
{
    auto createPlaceholder = [this, mapper = workbench()->mapper()](const QPoint& cell)
        {
            const QRectF geometry = mapper->mapFromGrid({cell, kCellSize});

            QSharedPointer<LayoutTourDropPlaceholder> result(new LayoutTourDropPlaceholder());
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

void LayoutTourReviewController::updateItemsLayout()
{
    if (!isLayoutTourReviewMode())
        return;

    const auto wbLayout = workbench()->currentLayout();
    const auto tourId = currentTourId();
    const auto tour = layoutTourManager()->tour(currentTourId());
    const auto reviewLayout = wbLayout->resource();
    NX_ASSERT(reviewLayout == m_reviewLayouts.value(tourId));

    nx::vms::api::LayoutTourDataList currentItems;

    auto layoutItems = wbLayout->items().values();
    QnWorkbenchItem::sortByGeometryAndName(layoutItems);

    for (auto layoutItem: layoutItems)
    {
        const bool unpinned = wbLayout->unpinItem(layoutItem);
        NX_ASSERT(unpinned);
    }

    const auto itemGrid = createItemGrid((int)tour.items.size());
    core::GridWalker walker(itemGrid);


    // Dynamically reorder existing items to match new order (if something was added or removed).
    for (const auto& item: tour.items)
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
                Qn::LayoutTourItemDelayMsRole,
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

void LayoutTourReviewController::resetReviewLayout(
    const LayoutResourcePtr& layout,
    const nx::vms::api::LayoutTourItemDataList& items)
{
    layout->cleanupItemData();
    layout->setItems(QnLayoutItemDataList());

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

void LayoutTourReviewController::addItemToReviewLayout(
    const LayoutResourcePtr& layout,
    const nx::vms::api::LayoutTourItemData& item,
    const QPointF& position,
    bool pinItem)
{
    if (layout->getItems().size() >= qnRuntime->maxSceneItems())
        return;

    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.combinedGeometry = QRectF(position, kCellSize);
    if (pinItem)
        itemData.flags = Qn::Pinned;
    itemData.resource.id = item.resourceId;
    layout->setItemData(itemData.uuid,
        Qn::LayoutTourItemDelayMsRole,
        item.delayMs);
    layout->addItem(itemData);
}

void LayoutTourReviewController::addResourcesToReviewLayout(
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

bool LayoutTourReviewController::fillTourItems(nx::vms::api::LayoutTourItemDataList* items)
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

        const auto delayMs = item->data(Qn::LayoutTourItemDelayMsRole).toInt();
        items->emplace_back(resource->getId(), delayMs);
    }

    return true;
}

void LayoutTourReviewController::handleItemDataChanged(
    const QnUuid& id,
    Qn::ItemDataRole role,
    const QVariant& data)
{
    if (role != Qn::LayoutTourItemDelayMsRole)
        return;

    // We can get here while changing layout.
    if (!isLayoutTourReviewMode())
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

void LayoutTourReviewController::at_reviewLayoutTourAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto id = parameters.argument<QnUuid>(Qn::UuidRole);
    reviewLayoutTour(layoutTourManager()->tour(id));
}

void LayoutTourReviewController::at_dropResourcesAction_triggered()
{
    if (!isLayoutTourReviewMode())
        return;

    const auto reviewLayout = workbench()->currentLayoutResource();
    const auto parameters = menu()->currentParameters(sender());
    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);
    addResourcesToReviewLayout(reviewLayout, parameters.resources(), position);
    const auto tour = layoutTourManager()->tour(currentTourId());
    menu()->trigger(action::SaveCurrentLayoutTourAction);
}

void LayoutTourReviewController::at_startCurrentLayoutTourAction_triggered()
{
    NX_ASSERT_HEAVY_CONDITION(isLayoutTourReviewMode());
    const auto tour = layoutTourManager()->tour(currentTourId());
    NX_ASSERT(tour.isValid());
    const auto startTourAction = action(action::ToggleLayoutTourModeAction);
    NX_ASSERT(!startTourAction->isChecked());
    if (!startTourAction->isChecked())
        menu()->trigger(action::ToggleLayoutTourModeAction, {Qn::UuidRole, tour.id});
}

void LayoutTourReviewController::at_saveCurrentLayoutTourAction_triggered()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    NX_ASSERT_HEAVY_CONDITION(isLayoutTourReviewMode());
    const auto id = currentTourId();
    auto tour = layoutTourManager()->tour(id);
    NX_ASSERT(tour.isValid());

    const auto reviewLayout = m_reviewLayouts.value(id);
    NX_ASSERT(reviewLayout);

    tour.items.clear();
    fillTourItems(&tour.items);
    tour.settings.manual = workbench()->currentLayout()->data(Qn::LayoutTourIsManualRole).toBool();
    layoutTourManager()->addOrUpdateTour(tour);
    qnClientCoreModule->layoutTourStateManager()->markChanged(tour.id, true);

    m_saveToursQueue.insert(tour.id);
    m_saveToursOperation->requestOperation();
}

void LayoutTourReviewController::at_removeCurrentLayoutTourAction_triggered()
{
    NX_ASSERT_HEAVY_CONDITION(isLayoutTourReviewMode());
    menu()->trigger(action::RemoveLayoutTourAction, {Qn::UuidRole, currentTourId()});
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
