#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/layout_resource.h>

#include <nx_ec/ec_api.h>

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
        [this](const QnUuid& tourId)
        {
            m_controller->stopTour(tourId);
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
            saveTourToServer(tour);
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
            saveTourToServer(tour);
        });

    connect(action(QnActions::RemoveLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            qnLayoutTourManager->removeTour(id);
            removeTourFromServer(id);
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
            saveTourToServer(tour);
        });

    connect(action(QnActions::ToggleLayoutTourModeAction), &QAction::triggered, this,
        [this](bool toggled)
        {
            if (!toggled)
            {
                m_controller->stopCurrentTour();
                return;
            }

            QnActionParameters parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (tour.isValid())
                m_controller->startTour(tour);
            else
                m_controller->startSingleLayoutTour();
        });

    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered, this,
        &LayoutToursHandler::openToursLayout);

    connect(action(QnActions::EscapeHotkeyAction), &QAction::triggered, m_controller,
        &LayoutTourController::stopCurrentTour);
}

LayoutToursHandler::~LayoutToursHandler()
{
}

void LayoutToursHandler::openToursLayout()
{
    const auto actions = QList<QnActions::IDType>()
        << QnActions::ToggleLayoutTourModeAction
        << QnActions::RemoveLayoutTourAction;

    const auto resource = QnLayoutResourcePtr(new QnLayoutResource());
    resource->setData(Qn::IsSpecialLayoutRole, true);
    resource->setData(Qn::LayoutIconRole, qnSkin->icon(lit("tree/videowall.png")));
    resource->setData(Qn::CustomPanelActionsRoleRole, QVariant::fromValue(actions));

    const auto startLayoutTourAction = action(QnActions::ToggleLayoutTourModeAction);
    const auto removeLayoutTourAction = action(QnActions::RemoveLayoutTourAction);

    const auto updateState =
        [startLayoutTourAction, removeLayoutTourAction, resource]()
        {
            const bool started = startLayoutTourAction->isChecked();
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

    resource->setId(QnUuid::createUuid());
    resourcePool()->addResource(resource);
    menu()->trigger(QnActions::OpenSingleLayoutAction, resource);
}

void LayoutToursHandler::saveTourToServer(const ec2::ApiLayoutTourData& tour)
{
    NX_EXPECT(qnLayoutTourManager->tour(tour.id).isValid());
    if (const auto connection = commonModule()->ec2Connection())
    {
        connection->getLayoutTourManager(Qn::kSystemAccess)->save(tour, this,
            [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});
    };
}

void LayoutToursHandler::removeTourFromServer(const QnUuid& tourId)
{
    if (const auto connection = commonModule()->ec2Connection())
    {
        connection->getLayoutTourManager(Qn::kSystemAccess)->remove(tourId, this,
            [](int /*reqId*/, ec2::ErrorCode /*errorCode*/) {});
    }
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
