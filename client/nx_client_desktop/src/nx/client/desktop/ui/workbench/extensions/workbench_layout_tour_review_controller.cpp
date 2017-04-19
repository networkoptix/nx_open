#include "workbench_layout_tour_review_controller.h"

#include <QtWidgets/QAction>

#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
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
            for (auto item : items)
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

            m_connections.reset(new QnDisconnectHelper());
            connectToLayout(workbench()->currentLayout());
            updateOrder();
        });

}

LayoutTourReviewController::~LayoutTourReviewController()
{
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
        &LayoutTourReviewController::connectToItem);
    *m_connections << connect(layout, &QnWorkbenchLayout::itemRemoved, this,
        [this](QnWorkbenchItem* item)
        {
            item->disconnect(this);
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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
