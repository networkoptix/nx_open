#include "workbench_layout_tour_review_controller.h"

#include <QtWidgets/QAction>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/layout_resource.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include <utils/common/uuid_pool.h>

namespace {

static const int kDefaultDelayMs = 5000;

const QnUuid uuidPoolBase("44a18151-242e-430b-8b57-4c94691902f9");
static const int kUuidsLimit = 16384;

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutTourReviewController::LayoutTourReviewController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_uuidPool(new QnUuidPool(uuidPoolBase, kUuidsLimit))
{
    connect(layoutTourManager(), &QnLayoutTourManager::tourChanged, this,
        &LayoutTourReviewController::handleTourChanged);

    connect(layoutTourManager(), &QnLayoutTourManager::tourRemoved, this,
        &LayoutTourReviewController::handleTourRemoved);

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource) { m_uuidPool->markAsUsed(resource->getId()); });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource) { m_uuidPool->markAsFree(resource->getId()); });

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
}

LayoutTourReviewController::~LayoutTourReviewController()
{
}

void LayoutTourReviewController::handleTourChanged(const ec2::ApiLayoutTourData& tour)
{
    // Handle only tours we are currently reviewing.
    auto reviewLayout = m_reviewLayouts.value(tour.id);
    if (!reviewLayout)
        return;

    reviewLayout->setData(Qn::CustomPanelTitleRole, tour.name);
    reviewLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    if (auto wbLayout = QnWorkbenchLayout::instance(reviewLayout))
        wbLayout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);

    cleanupAutoGeneratedLayouts(tour);

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
    // Handle only tours we are currently reviewing.
    if (auto reviewLayout = m_reviewLayouts.take(tourId))
        resourcePool()->removeResource(reviewLayout);
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
}

void LayoutTourReviewController::stopListeningLayout()
{
    m_connections.reset();
}

void LayoutTourReviewController::reviewLayoutTour(const ec2::ApiLayoutTourData& tour)
{
    if (!tour.isValid())
        return;

    if (auto layout = m_reviewLayouts.value(tour.id))
    {
        menu()->trigger(action::OpenSingleLayoutAction, layout);
        return;
    }

    const QList<action::IDType> actions{
        action::StartCurrentLayoutTourAction,
        action::SaveCurrentLayoutTourAction,
        action::RemoveCurrentLayoutTourAction
    };

    static const float kCellAspectRatio{4.0f / 3.0f};

    const auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid()); //< Layout is never saved to server
    layout->setParentId(tour.id);
    layout->setName(tour.name);
    layout->setData(Qn::IsSpecialLayoutRole, true);
    layout->setData(Qn::LayoutIconRole, qnSkin->icon(lit("tree/videowall.png")));
    layout->setData(Qn::CustomPanelActionsRole, qVariantFromValue(actions));
    layout->setData(Qn::CustomPanelTitleRole, tour.name);
    layout->setData(Qn::CustomPanelDescriptionRole, QString());
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadWriteSavePermission
        | Qn::AddRemoveItemsPermission));
    layout->setData(Qn::LayoutTourUuidRole, qVariantFromValue(tour.id));
    layout->setData(Qn::LayoutTourIsManualRole, tour.settings.manual);
    layout->setCellAspectRatio(kCellAspectRatio);

    for (auto item : tour.items)
        addItemToReviewLayout(layout, item);

    resourcePool()->addResource(layout);

    m_reviewLayouts.insert(tour.id, layout);

    menu()->trigger(action::OpenSingleLayoutAction, layout);
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

    *m_connections << connect(layout, &QnWorkbenchLayout::itemAdded, this,
        [this, layout](QnWorkbenchItem* item)
        {
            connectToItem(item);
            updateOrder();
            updateButtons(layout->resource());
        });
    *m_connections << connect(layout, &QnWorkbenchLayout::itemRemoved, this,
        [this, layout](QnWorkbenchItem* item)
        {
            item->disconnect(this);
            updateOrder();
            updateButtons(layout->resource());
        });
    for (auto item: layout->items())
        connectToItem(item);
}

void LayoutTourReviewController::connectToItem(QnWorkbenchItem* item)
{
    *m_connections << connect(item, &QnWorkbenchItem::geometryChanged, this,
        &LayoutTourReviewController::updateOrder);
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

    // Using he fact that dataChanged will be sent even if action list was not changed
    const QList<action::IDType> actions{
        action::StartCurrentLayoutTourAction,
        action::SaveCurrentLayoutTourAction,
        action::RemoveCurrentLayoutTourAction
    };
    layout->setData(Qn::CustomPanelActionsRole, qVariantFromValue(actions));
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
    itemData.resource.id = item.layoutId;
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
        const auto layout = resourcePool()->getResourceByUniqueId(item->resourceUid());
        NX_EXPECT(layout);
        if (!layout)
            continue;

        const auto delayMs = item->data(Qn::LayoutTourItemDelayMsRole).toInt();
        items->emplace_back(layout->getId(), delayMs);
    }

    return true;
}

QnLayoutResourcePtr LayoutTourReviewController::constructLayout(const QnResourcePtr& resource) const
{
    auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(m_uuidPool->getFreeId());
    layout->addFlags(Qn::local);
    layout->setCellSpacing(0);
    layout->setName(resource->getName());

    QnLayoutItemData item;
    item.flags = Qn::Pinned;
    item.uuid = QnUuid::createUuid();
    item.combinedGeometry = QRect(0, 0, 1, 1);
    item.resource.id = resource->getId();
    item.resource.uniqueId = resource->getUniqueId();
    layout->addItem(item);

    return layout;
}

void LayoutTourReviewController::cleanupAutoGeneratedLayouts(const ec2::ApiLayoutTourData& tour)
{
    const auto layouts = resourcePool()->getResourcesByParentId(tour.id).filtered<QnLayoutResource>();

    QSet<QnUuid> used;
    for (const auto& item: tour.items)
        used << item.layoutId;

    // Ignore review layouts
    if (auto reviewLayout = m_reviewLayouts.value(tour.id))
        used << reviewLayout->getId();

    for (const auto& layout: layouts)
    {
        // Skip layouts that are still not saved on server.
        if (layout->hasFlags(Qn::local))
            continue;

        if (used.contains(layout->getId()))
            continue;

        menu()->trigger(action::RemoveFromServerAction, layout);
    }
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
            const bool allowed = QnResourceAccessFilter::isShareableMedia(resource)
                || resource->hasFlags(Qn::local_media);

            if (!allowed)
                continue;

            const auto layout = constructLayout(resource);
            layout->setParentId(tourId);
            resourcePool()->addResource(layout);
            snapshotManager()->save(layout);
            addItemToReviewLayout(reviewLayout, {layout->getId(), kDefaultDelayMs}, position);
        }

    }
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

    tour.items.clear();
    fillTourItems(&tour.items);
    tour.settings.manual = workbench()->currentLayout()->data(Qn::LayoutTourIsManualRole).toBool();
    layoutTourManager()->addOrUpdateTour(tour);
    menu()->trigger(action::SaveLayoutTourAction, {Qn::UuidRole, id});
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
