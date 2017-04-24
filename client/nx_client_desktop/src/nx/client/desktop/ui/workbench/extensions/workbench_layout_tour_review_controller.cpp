#include "workbench_layout_tour_review_controller.h"

#include <QtWidgets/QAction>

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

namespace {

static const int kDefaultDelayMs = 5000;

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
    connect(qnLayoutTourManager, &QnLayoutTourManager::tourChanged, this,
        [this](const ec2::ApiLayoutTourData& tour)
        {
            if (auto reviewLayout = m_reviewLayouts.value(tour.id))
            {
                reviewLayout->setData(Qn::CustomPanelTitleRole, tour.name);

                ec2::ApiLayoutTourItemDataList currentItems;
                if (fillTourItems(&currentItems) && currentItems == tour.items)
                    return;

                reviewLayout->setItems(QnLayoutItemDataList());
                for (auto item: tour.items)
                    addItemToReviewLayout(reviewLayout, item);
                updateButtons(reviewLayout);
            }
        });

    connect(qnLayoutTourManager, &QnLayoutTourManager::tourRemoved, this,
        [this](const QnUuid& tourId)
        {
            if (auto reviewLayout = m_reviewLayouts.take(tourId))
                resourcePool()->removeResource(reviewLayout);
        });

    connect(action(action::ReviewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            reviewLayoutTour(qnLayoutTourManager->tour(id));
        });

    connect(action(action::DropResourcesAction), &QAction::triggered, this,
        [this]()
        {
            if (!isLayoutTourReviewMode())
                return;

            auto reviewLayout = m_reviewLayouts.value(currentTourId());
            NX_EXPECT(reviewLayout);
            if (!reviewLayout)
                return;

            const auto parameters = menu()->currentParameters(sender());
            QPointF position = parameters.argument<QPointF>(Qn::ItemPositionRole);

            for (const auto& layout: parameters.resources().filtered<QnLayoutResource>())
                addItemToReviewLayout(reviewLayout, {layout->getId(), kDefaultDelayMs}, position);
        });

    connect(action(action::StartCurrentLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            NX_EXPECT(isLayoutTourReviewMode());
            const auto tour = qnLayoutTourManager->tour(currentTourId());
            NX_EXPECT(tour.isValid());
            const auto startTourAction = action(action::ToggleLayoutTourModeAction);
            NX_EXPECT(!startTourAction->isChecked());
            if (!startTourAction->isChecked())
            {
                menu()->trigger(action::ToggleLayoutTourModeAction, {Qn::UuidRole, tour.id});
            }
        });

    connect(action(action::SaveCurrentLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            NX_EXPECT(isLayoutTourReviewMode());
            const auto id = currentTourId();
            auto tour = qnLayoutTourManager->tour(id);
            NX_EXPECT(tour.isValid());

            tour.items.clear();
            fillTourItems(&tour.items);
            qnLayoutTourManager->addOrUpdateTour(tour);
            menu()->trigger(action::SaveLayoutTourAction, {Qn::UuidRole, id});
        });

    connect(action(action::RemoveCurrentLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            NX_EXPECT(isLayoutTourReviewMode());
            menu()->trigger(action::RemoveLayoutTourAction, {Qn::UuidRole, currentTourId()});
        });

    connect(workbench(), &QnWorkbench::currentLayoutAboutToBeChanged, this,
        [this]
        {
            m_connections.reset();
        });

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        [this]
        {
            if (!isLayoutTourReviewMode())
                return;

            NX_EXPECT(m_reviewLayouts.values().contains(workbench()->currentLayout()->resource()));
            m_connections.reset(new QnDisconnectHelper());
            connectToLayout(workbench()->currentLayout());
            updateOrder();
            updateButtons(workbench()->currentLayout()->resource());
        });

}

LayoutTourReviewController::~LayoutTourReviewController()
{
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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
