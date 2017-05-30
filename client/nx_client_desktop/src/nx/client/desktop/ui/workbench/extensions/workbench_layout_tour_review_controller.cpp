#include "workbench_layout_tour_review_controller.h"

#include <QtWidgets/QAction>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/layout_resource.h>

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
#include <utils/common/pending_operation.h>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace {

static const int kDefaultDelayMs = 5000;

// Save layout tour not more often than once in 5 seconds.
static const int kSaveTourIntervalMs = 5000;

} // namespace

namespace nx {
namespace client {
namespace desktop {
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
    m_saveToursOperation = new QnPendingOperation(saveQueuedTours, kSaveTourIntervalMs, this);

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
    if (auto wbLayout = QnWorkbenchLayout::instance(reviewLayout))
        wbLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);

    // Check if items were not changed (e.g. we have just changed them ourselves).
    ec2::ApiLayoutTourItemDataList currentItems;
    if (fillTourItems(&currentItems) && currentItems == tour.items)
        return;

    // Update items if someone else have changed the tour.
    for (const auto& itemId: reviewLayout->getItems().keys())
        qnResourceRuntimeDataManager->cleanupData(itemId);
    reviewLayout->setItems(QnLayoutItemDataList());
    for (const auto& item: tour.items)
        addItemToReviewLayout(reviewLayout, item);
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
        | QnLayoutFlag::NoTimeline));
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::LayoutTourUuidRole, qVariantFromValue(tour.id));
    layout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    layout->setCellAspectRatio(kCellAspectRatio);

    for (auto item: tour.items)
        addItemToReviewLayout(layout, item);

    m_reviewLayouts.insert(tour.id, layout);

    menu()->trigger(action::OpenInNewTabAction, layout);
    updateButtons(layout);
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

    auto saveAction = action(action::SaveCurrentLayoutTourAction);

    *m_connections << connect(layout, &QnWorkbenchLayout::itemAdded, saveAction, &QAction::trigger);
    *m_connections << connect(layout, &QnWorkbenchLayout::itemMoved, saveAction, &QAction::trigger);
    *m_connections << connect(layout, &QnWorkbenchLayout::itemRemoved, saveAction, &QAction::trigger);
    *m_connections << connect(layout, &QnWorkbenchLayout::boundingRectChanged, this,
        &LayoutTourReviewController::updatePlaceholders);
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
    static const QSize kPlaceholderSize(1, 1);

    auto createPlaceholder = [this, mapper = workbench()->mapper()](const QPoint& cell)
        {
            const QRectF geometry = mapper->mapFromGrid({cell, kPlaceholderSize});

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

    QRect boundingRect = layout->boundingRect();

    // Layout can have valid bounds after removing the last item, handling it here to improve UI.
    if (!boundingRect.isValid() || itemCount == 0)
        boundingRect = QRect(0, 0, 1, 1);

    static const int kMaxSize = 4;
    if (boundingRect.width() > kMaxSize || boundingRect.height() > kMaxSize)
    {
        m_dropPlaceholders.clear();
        return;
    }

    int minWidth = 2;
    int minHeight = 2;

    if (itemCount > 3)
        minHeight = 3;
    if (itemCount > 5)
        minWidth = 3;
    if (itemCount > 8)
        minHeight = 4;

    if (boundingRect.width() < minWidth)
        boundingRect.setWidth(minWidth);
    if (boundingRect.height() < minHeight)
        boundingRect.setHeight(minHeight);

    for (const auto& p: m_dropPlaceholders.keys())
    {
        if (!boundingRect.contains(p))
            m_dropPlaceholders.remove(p);
    }

    for (int x = boundingRect.left(); x <= boundingRect.right(); ++x)
    {
        for (int y = boundingRect.top(); y <= boundingRect.bottom(); ++y)
        {
            const QPoint cell(x, y);
            const bool isFree = layout->isFreeSlot(cell, kPlaceholderSize);
            const bool placeholderExists = m_dropPlaceholders.contains(cell);

            // If cell is empty and there is a placeholder (or vise versa), skip this step.
            if (isFree == placeholderExists)
                continue;

            if (placeholderExists)
                m_dropPlaceholders.remove(cell);
            else
                m_dropPlaceholders.insert(cell, createPlaceholder(cell));
        }
    }

    layout->setData(Qn::LayoutMinimalBoundingRectRole, boundingRect);
    display()->fitInView(display()->animationAllowed());
}

void LayoutTourReviewController::addItemToReviewLayout(
    const QnLayoutResourcePtr& layout,
    const ec2::ApiLayoutTourItemData& item,
    const QPointF& position)
{
    static const int kMatrixWidth = 3;
    const int index = layout->getItems().size();

    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();

    if (!position.isNull())
    {
        itemData.combinedGeometry = QRectF(position, position);
    }
    else
    {
        QPointF bestPos(index % kMatrixWidth, index / kMatrixWidth);
        itemData.combinedGeometry = QRectF(bestPos, bestPos);
    }
    itemData.flags = Qn::PendingGeometryAdjustment;
    itemData.resource.id = item.resourceId;
    qnResourceRuntimeDataManager->setLayoutItemData(itemData.uuid,
        Qn::LayoutTourItemDelayMsRole, item.delayMs);

    layout->addItem(itemData);
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
        const auto resource = resourcePool()->getResourceByUniqueId(item->resourceUid());
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

    const auto tourId = currentTourId();

    auto reviewLayout = m_reviewLayouts.value(tourId);
    NX_EXPECT(reviewLayout);
    if (!reviewLayout)
        return;

    const auto parameters = menu()->currentParameters(sender());
    QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

    for (const auto& resource: parameters.resources())
    {
        if (resource->hasFlags(Qn::layout))
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_EXPECT(layout);
            addItemToReviewLayout(reviewLayout, {layout->getId(), kDefaultDelayMs}, position);
        }
        else
        {
            const bool allowed = QnResourceAccessFilter::isOpenableInLayout(resource);

            if (!allowed)
                continue;

            addItemToReviewLayout(reviewLayout, {resource->getId(), kDefaultDelayMs}, position);
        }

    }
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

    updateOrder();
    updateButtons(reviewLayout);
    updatePlaceholders();

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
