#include "workbench_layout_tour_review_controller.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QAction>

#include <client_core/client_core_module.h>

#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/layout_resource.h>

#include <nx/client/core/utils/grid_walker.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/graphics/items/resource/layout_tour_drop_placeholder.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_utility.h>

#include <utils/common/delayed.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/pending_operation.h>

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

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

static bool onlyOnePlaceholder = false;

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
        new utils::PendingOperation(saveQueuedTours, kSaveTourIntervalMs, this);

    auto updateItemsLayout = [this]
        {
            this->updateItemsLayout();
        };
    m_updateItemsLayoutOperation =
        new utils::PendingOperation(updateItemsLayout, kUpdateItemsLayoutIntervalMs, this);

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

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        &LayoutTourReviewController::stopListeningLayout);

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        &LayoutTourReviewController::startListeningLayout);

    connect(qnResourceRuntimeDataManager, &QnResourceRuntimeDataManager::layoutItemDataChanged,
        this, &LayoutTourReviewController::handleItemDataChanged);

    connect(action(action::DebugIncrementCounterAction), &QAction::triggered, this,
        [this]{ onlyOnePlaceholder = !onlyOnePlaceholder; updatePlaceholders(); });
}

LayoutTourReviewController::~LayoutTourReviewController()
{
    stopListeningLayout();
}

void LayoutTourReviewController::handleTourChanged(const ec2::ApiLayoutTourData& tour)
{
    // Handle only tours we are currently reviewing.
    auto reviewLayout = m_reviewLayouts.value(tour.id);
    if (!reviewLayout)
        return;

    reviewLayout->setName(tour.name);
    reviewLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    auto wbLayout = QnWorkbenchLayout::instance(reviewLayout);
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
        if (auto wbLayout = QnWorkbenchLayout::instance(reviewLayout))
        {
            wbLayout->deleteLater();
            workbench()->removeLayout(wbLayout);
            if (workbench()->layouts().empty())
                menu()->trigger(action::OpenNewTabAction);
        }
    }
}

void LayoutTourReviewController::startListeningLayout()
{
    if (!isLayoutTourReviewMode())
        return;

    NX_EXPECT(m_reviewLayouts.values().contains(workbench()->currentLayout()->resource()));
    m_connections.reset(new QnDisconnectHelper());
    connectToLayout(workbench()->currentLayout());
    updateOrder();
    updateButtons(workbench()->currentLayout()->resource());
    updatePlaceholders();
}

void LayoutTourReviewController::stopListeningLayout()
{
    m_connections.reset();
    m_dropPlaceholders.clear();
}

void LayoutTourReviewController::reviewLayoutTour(const ec2::ApiLayoutTourData& tour)
{
    if (!tour.isValid())
        return;

    if (auto layout = m_reviewLayouts.value(tour.id))
    {
        menu()->trigger(action::OpenInNewTabAction, layout);
        return;
    }

    const QList<action::IDType> actions{action::StartCurrentLayoutTourAction};

    static const float kCellAspectRatio{16.0f / 9.0f};

    const auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid()); //< Layout is never saved to server.
    layout->setParentId(tour.id);
    layout->setName(tour.name);
    layout->setData(Qn::IsSpecialLayoutRole, true);
    layout->setData(Qn::LayoutIconRole, qnResIconCache->icon(QnResourceIconCache::LayoutTour));
    layout->setData(Qn::LayoutFlagsRole, qVariantFromValue(QnLayoutFlag::FixedViewport
        | QnLayoutFlag::NoResize
        | QnLayoutFlag::NoTimeline
        | QnLayoutFlag::FillViewport
    ));
    layout->setData(Qn::LayoutMarginsRole, qVariantFromValue(kReviewMargins));
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::LayoutTourUuidRole, qVariantFromValue(tour.id));
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
    if (!layout->resource())
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
    *m_connections << connect(layout, &QnWorkbenchLayout::itemAdded, this, saveAndUpdateLayout,
        Qt::QueuedConnection);
    *m_connections << connect(layout, &QnWorkbenchLayout::itemsMoved, this, saveAndUpdateLayout,
        Qt::QueuedConnection);
    *m_connections << connect(layout, &QnWorkbenchLayout::itemRemoved, this, saveAndUpdateLayout,
        Qt::QueuedConnection);
}

void LayoutTourReviewController::updateOrder()
{
    NX_EXPECT(isLayoutTourReviewMode());

    auto items = workbench()->currentLayout()->items().toList();
    QnWorkbenchItem::sortByGeometry(&items);

    int index = 0;
    for (auto item: items)
        item->setData(Qn::LayoutTourItemOrderRole, ++index);
}

void LayoutTourReviewController::updateButtons(const QnLayoutResourcePtr& layout)
{
    NX_EXPECT(layout);
    if (!layout)
        return;

    // Using he fact that dataChanged will be sent even if action list was not changed.
    const QList<action::IDType> actions{action::StartCurrentLayoutTourAction};
    layout->setData(Qn::CustomPanelActionsRole, qVariantFromValue(actions));
}

void LayoutTourReviewController::updatePlaceholders()
{
    auto createPlaceholder = [this, mapper = workbench()->mapper()](const QPoint& cell)
        {
            const QRectF geometry = mapper->mapFromGrid({cell, kCellSize});

            QSharedPointer<LayoutTourDropPlaceholder> result(new LayoutTourDropPlaceholder());
            result->setRect(geometry);
            display()->setLayer(result.data(), Qn::BackLayer);
            display()->scene()->addItem(result.data());
            return result;
        };

    const auto layout = workbench()->currentLayout();
    if (!layout->resource())
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
            const bool mustBePlaceholder = isFree &&
                (onlyOnePlaceholder ? placeholdersCount == 0 : true);

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
    NX_EXPECT(reviewLayout == m_reviewLayouts.value(tourId));

    ec2::ApiLayoutTourItemDataList currentItems;

    auto layoutItems = wbLayout->items().toList();
    QnWorkbenchItem::sortByGeometry(&layoutItems);

    for (auto layoutItem : layoutItems)
    {
        const bool unpinned = wbLayout->unpinItem(layoutItem);
        NX_EXPECT(unpinned);
    }

    const auto itemGrid = createItemGrid((int)tour.items.size());
    core::GridWalker walker(itemGrid);


    // Dynamically reorder existing items to match new order (if something was added or removed).
    for (const auto& item: tour.items)
    {
        // Find first existing item that match the resource.
        auto existing = std::find_if(layoutItems.begin(), layoutItems.end(),
            [this, item](QnWorkbenchItem* existing)
            {
                const auto resource = existing->resource();
                return resource && resource->getId() == item.resourceId;
            });

        const bool hasPosition = walker.next();
        NX_EXPECT(hasPosition);
        // Move existing item to the selected place.
        if (existing != layoutItems.end())
        {
            auto layoutItem = *existing;
            const bool pinned = wbLayout->pinItem(layoutItem, {walker.pos(), kCellSize});
            NX_EXPECT(pinned);
            qnResourceRuntimeDataManager->setLayoutItemData(layoutItem->uuid(),
                Qn::LayoutTourItemDelayMsRole, item.delayMs);
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
        qnResourceRuntimeDataManager->cleanupData(layoutItem->uuid());
        reviewLayout->removeItem(layoutItem->uuid());
    }

    updatePlaceholders();
    updateOrder();
}

void LayoutTourReviewController::resetReviewLayout(const QnLayoutResourcePtr& layout,
    const ec2::ApiLayoutTourItemDataList& items)
{
    for (const auto& itemId: layout->getItems().keys())
        qnResourceRuntimeDataManager->cleanupData(itemId);
    layout->setItems(QnLayoutItemDataList());

    const int gridSize = std::min((int)items.size(), qnRuntime->maxSceneItems());

    const auto grid = createItemGrid(gridSize);
    GridWalker walker(grid);

    for (const auto& item: items)
    {
        addItemToReviewLayout(layout, item, walker.pos(), true);
        walker.next();
    }
    updateButtons(layout);
}

void LayoutTourReviewController::addItemToReviewLayout(
    const QnLayoutResourcePtr& layout,
    const ec2::ApiLayoutTourItemData& item,
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
    qnResourceRuntimeDataManager->setLayoutItemData(itemData.uuid,
        Qn::LayoutTourItemDelayMsRole, item.delayMs);
    layout->addItem(itemData);
}

void LayoutTourReviewController::addResourcesToReviewLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& resources,
    const QPointF& position)
{
    NX_EXPECT(!m_updating);
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    for (const auto& resource: resources.filtered(QnResourceAccessFilter::isOpenableInEntity))
        addItemToReviewLayout(layout, {resource->getId(), kDefaultDelayMs}, position, false);
}

bool LayoutTourReviewController::fillTourItems(ec2::ApiLayoutTourItemDataList* items)
{
    NX_EXPECT(items);
    if (!items)
        return false;

    auto layoutItems = workbench()->currentLayout()->items().toList();
    QnWorkbenchItem::sortByGeometry(&layoutItems);
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

void LayoutTourReviewController::handleItemDataChanged(const QnUuid& id, Qn::ItemDataRole role,
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

    selectedItems.remove(id);
    for (const auto& itemId: selectedItems)
        qnResourceRuntimeDataManager->setLayoutItemData(itemId, role, data);
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

    const auto reviewLayout = workbench()->currentLayout()->resource();
    const auto parameters = menu()->currentParameters(sender());
    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);
    addResourcesToReviewLayout(reviewLayout, parameters.resources(), position);
    const auto tour = layoutTourManager()->tour(currentTourId());
    menu()->trigger(action::SaveCurrentLayoutTourAction);
}

void LayoutTourReviewController::at_startCurrentLayoutTourAction_triggered()
{
    NX_EXPECT(isLayoutTourReviewMode());
    const auto tour = layoutTourManager()->tour(currentTourId());
    NX_EXPECT(tour.isValid());
    const auto startTourAction = action(action::ToggleLayoutTourModeAction);
    NX_EXPECT(!startTourAction->isChecked());
    if (!startTourAction->isChecked())
        menu()->trigger(action::ToggleLayoutTourModeAction, {Qn::UuidRole, tour.id});
}

void LayoutTourReviewController::at_saveCurrentLayoutTourAction_triggered()
{
    if (m_updating)
        return;
    QScopedValueRollback<bool> guard(m_updating, true);

    NX_EXPECT(isLayoutTourReviewMode());
    const auto id = currentTourId();
    auto tour = layoutTourManager()->tour(id);
    NX_EXPECT(tour.isValid());

    const auto reviewLayout = m_reviewLayouts.value(id);
    NX_EXPECT(reviewLayout);

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
    NX_EXPECT(isLayoutTourReviewMode());
    menu()->trigger(action::RemoveLayoutTourAction, {Qn::UuidRole, currentTourId()});
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
