#include "workbench_layout_tour_review_controller.h"

#include <QtWidgets/QAction>

#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/layout_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

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
                //TODO: #GDM #3.1 dynamically change items
                reviewLayout->setData(Qn::CustomPanelTitleRole, tour.name);
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

    connect(action(QnActions::ReviewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            reviewLayoutTour(qnLayoutTourManager->tour(id));
        });

    connect(action(QnActions::DropResourcesAction), &QAction::triggered, this,
        [this]()
        {
            if (!isLayoutTourReviewMode())
                return;

            QnActionParameters parameters = menu()->currentParameters(sender());
            auto tour = qnLayoutTourManager->tour(currentTourId());
            for (const auto& layout: parameters.resources().filtered<QnLayoutResource>())
            {
                tour.items.emplace_back(layout->getId(), 5000);
            }
            qnLayoutTourManager->addOrUpdateTour(tour);

        });

    connect(action(QnActions::StartCurrentLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            NX_EXPECT(isLayoutTourReviewMode());
            const auto tour = qnLayoutTourManager->tour(currentTourId());
            NX_EXPECT(tour.isValid());
            const auto startTourAction = action(QnActions::ToggleLayoutTourModeAction);
            NX_EXPECT(!startTourAction->isChecked());
            if (!startTourAction->isChecked())
            {
                menu()->trigger(QnActions::ToggleLayoutTourModeAction, QnActionParameters()
                    .withArgument(Qn::UuidRole, tour.id));
            }
        });

    connect(action(QnActions::SaveCurrentLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            NX_EXPECT(isLayoutTourReviewMode());
            const auto id = currentTourId();
            auto tour = qnLayoutTourManager->tour(id);
            NX_EXPECT(tour.isValid());

            auto items = workbench()->currentLayout()->items().toList();
            QnWorkbenchItem::sortByGeometry(&items);
            tour.items.clear();
            for (auto item: items)
            {
                const auto layout = resourcePool()->getResourceByUniqueId(item->resourceUid());
                NX_EXPECT(layout);
                if (!layout)
                    continue;

                const auto delayMs = item->data(Qn::LayoutTourItemDelayMsRole).toInt();
                tour.items.emplace_back(layout->getId(), delayMs);
            }

            qnLayoutTourManager->addOrUpdateTour(tour);
            menu()->trigger(QnActions::SaveLayoutTourAction, QnActionParameters()
                .withArgument(Qn::UuidRole, id));
        });

    connect(action(QnActions::RemoveCurrentLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            NX_EXPECT(isLayoutTourReviewMode());

            menu()->trigger(QnActions::RemoveLayoutTourAction, QnActionParameters()
                .withArgument(Qn::UuidRole, currentTourId()));
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
        menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
        return;
    }

    const QList<QnActions::IDType> actions{
        QnActions::StartCurrentLayoutTourAction,
        QnActions::SaveCurrentLayoutTourAction,
        QnActions::RemoveCurrentLayoutTourAction
    };

    static const float kCellAspectRatio{4.0f / 3.0f};

    const auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid()); //< Layout is never saved to server
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
    resourcePool()->markLayoutAutoGenerated(layout); //TODO: #GDM #3.1 simplify scheme for local layouts

    m_reviewLayouts.insert(tour.id, layout);

    menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
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
    const QList<QnActions::IDType> actions{
        QnActions::StartCurrentLayoutTourAction,
        QnActions::SaveCurrentLayoutTourAction,
        QnActions::RemoveCurrentLayoutTourAction
    };
    layout->setData(Qn::CustomPanelActionsRole, qVariantFromValue(actions));
}

void LayoutTourReviewController::addItemToReviewLayout(const QnLayoutResourcePtr& layout,
    const ec2::ApiLayoutTourItemData& item)
{
    static const int kMatrixWidth = 3;
    const int index = layout->getItems().size();

    QnLayoutItemData itemData;
    itemData.uuid = QnUuid::createUuid();
    itemData.combinedGeometry = QRect(index % kMatrixWidth, index / kMatrixWidth, 1, 1);
    itemData.flags = Qn::Pinned;
    itemData.resource.id = item.layoutId;
    qnResourceRuntimeDataManager->setLayoutItemData(itemData.uuid,
        Qn::LayoutTourItemDelayMsRole, item.delayMs);

    layout->addItem(itemData);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
