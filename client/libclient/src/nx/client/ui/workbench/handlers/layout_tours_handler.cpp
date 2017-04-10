#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/layout_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/layout_tour_dialog.h>
#include <ui/style/skin.h>
#include <ui/workbench/extensions/workbench_layout_tour_controller.h>

#include <nx/utils/string.h>


namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_controller(new LayoutTourController(this))
{
    connect(qnLayoutTourManager, &QnLayoutTourManager::tourChanged, m_controller,
        &LayoutTourController::updateTour);

    connect(qnLayoutTourManager, &QnLayoutTourManager::tourRemoved, this,
        [this](const ec2::ApiLayoutTourData& tour)
        {
            m_controller->stopTour(tour.id);
        });

    connect(action(QnActions::NewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QStringList usedNames;
            for (const auto& tour: qnLayoutTourManager->tours())
                usedNames << tour.name;

            ec2::ApiLayoutTourData tour;
            tour.id = QnUuid::createUuid();
            tour.name = nx::utils::generateUniqueString(
                usedNames, tr("Layout Tour"), tr("Layout Tour %1"));
            qnLayoutTourManager->addOrUpdateTour(tour);
        });

    connect(action(QnActions::RenameLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            tour.name = parameters.argument<QString>(Qn::ResourceNameRole);
            qnLayoutTourManager->addOrUpdateTour(tour);
            qnLayoutTourManager->saveTour(tour);
        });

    connect(action(QnActions::RemoveLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            qnLayoutTourManager->removeTour(tour);
        });

    connect(action(QnActions::LayoutTourSettingsAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;

            QScopedPointer<QnLayoutTourDialog> dialog(new QnLayoutTourDialog(mainWindow()));
            dialog->loadData(tour);
            if (!dialog->exec())
                return;

            dialog->submitData(&tour);
            qnLayoutTourManager->addOrUpdateTour(tour);
            qnLayoutTourManager->saveTour(tour);
        });

    connect(action(QnActions::StartLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            m_controller->startTour(tour);
        });

    connect(action(QnActions::StopLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            m_controller->stopTour(m_controller->runningTour());
        });

    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered, this,
        &LayoutToursHandler::openToursLayout);

    connect(action(QnActions::ToggleTourModeAction), &QAction::triggered, m_controller,
        &LayoutTourController::toggleLayoutTour);
}

LayoutToursHandler::~LayoutToursHandler()
{
}

void LayoutToursHandler::openToursLayout()
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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
