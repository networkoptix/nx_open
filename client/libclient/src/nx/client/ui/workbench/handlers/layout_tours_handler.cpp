#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/actions/action_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace ui {
namespace workbench {
namespace handlers {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered,
        this, &LayoutToursHandler::openTousLayout);
}

void LayoutToursHandler::openTousLayout()
{
    const auto actions = QList<QnActions::IDType>()
        << QnActions::StartLayoutTourAction
        << QnActions::StopLayoutTourAction
        << QnActions::RemoveLayoutTourAction;

    const auto resource = QnLayoutResourcePtr(new QnLayoutResource());
    resource->setData(Qn::IsSpecialLayoutRole, true);
    resource->setData(Qn::LayoutIconRole, qnSkin->icon(lit("tree/videowall.png")));
    resource->setData(Qn::CustomPanelActionsRoleRole, QVariant::fromValue(actions));

    const auto startLayoutTourAction = action(QnActions::StartLayoutTourAction);
    const auto stopLayoutTourAction = action(QnActions::StopLayoutTourAction);
    const auto removeLayoutTourAction = action(QnActions::RemoveLayoutTourAction);

    startLayoutTourAction->setChecked(false);
    const auto updateState =
        [startLayoutTourAction, stopLayoutTourAction, removeLayoutTourAction, resource]()
        {
            const bool started = startLayoutTourAction->isChecked();
            startLayoutTourAction->setVisible(!started);
            stopLayoutTourAction->setEnabled(started);
            stopLayoutTourAction->setVisible(started);
            removeLayoutTourAction->setEnabled(!started);

            static const auto kStarted = tr(" (STARTED)");
            resource->setData(Qn::CustomPanelTitleRole, tr("Super Duper Tours Layout Panel")
                + (started ? kStarted : QString()));
            resource->setData(Qn::CustomPanelDescriptionRole, tr("Description Of Tour")
                + (started ? kStarted : QString()));
            resource->setName(lit("Test Layout Tours") + (started ? kStarted : QString()));
        };

    updateState();
    connect(startLayoutTourAction, &QAction::toggled, this, updateState);
    connect(stopLayoutTourAction, &QAction::triggered, this,
        [startLayoutTourAction]() { startLayoutTourAction->setChecked(false); });

    resource->setId(QnUuid::createUuid());
    qnResPool->addResource(resource);
    menu()->trigger(QnActions::OpenSingleLayoutAction, resource);
}

} // namespace handlers
} // namespace workbench
} // namespace ui
} // namespace client
} // namespace nx
