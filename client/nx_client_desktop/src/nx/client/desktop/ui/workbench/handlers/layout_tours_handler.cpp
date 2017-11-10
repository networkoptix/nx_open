#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>
#include <core/resource/user_resource.h>

#include <nx_ec/ec_api.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/client/desktop/ui/messages/resources_messages.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>
#include <nx/client/desktop/ui/workbench/extensions/workbench_layout_tour_executor.h>
#include <nx/client/desktop/ui/workbench/extensions/workbench_layout_tour_review_controller.h>

#include <nx/utils/string.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_tourExecutor(new LayoutTourExecutor(this)),
    m_reviewController(new LayoutTourReviewController(this))
{
    connect(layoutTourManager(), &QnLayoutTourManager::tourChanged, m_tourExecutor,
        &LayoutTourExecutor::updateTour);

    connect(layoutTourManager(), &QnLayoutTourManager::tourRemoved, m_tourExecutor,
        &LayoutTourExecutor::stopTour);

    connect(action(action::NewLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            NX_EXPECT(context()->user());
            if (!context()->user())
                return;

            QStringList usedNames;
            for (const auto& tour: layoutTourManager()->tours())
                usedNames << tour.name;

            ec2::ApiLayoutTourData tour;
            tour.id = QnUuid::createUuid();
            tour.parentId = context()->user()->getId();
            tour.name = nx::utils::generateUniqueString(
                usedNames, tr("Showreel"), tr("Showreel %1"));
            layoutTourManager()->addOrUpdateTour(tour);
            saveTourToServer(tour);
            menu()->trigger(action::SelectNewItemAction, {Qn::UuidRole, tour.id});
            menu()->trigger(action::ReviewLayoutTourAction, {Qn::UuidRole, tour.id});
        });

    connect(action(action::MakeLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            if (!menu()->triggerIfPossible(action::NewLayoutTourAction))
                return;
            const auto parameters = menu()->currentParameters(sender());
            menu()->trigger(action::DropResourcesAction, parameters);
            menu()->trigger(action::SaveCurrentLayoutTourAction);
        });

    connect(action(action::RenameLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = layoutTourManager()->tour(id);
            if (!tour.isValid())
                return;

            const auto userId = context()->user()->getId();

            const QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

            // Ask to override tour with the same name (if any). Create local copy to avoid crash.
            const auto tours = layoutTourManager()->tours();
            for (const auto& other: tours)
            {
                if (other.id == id)
                    continue;

                if (other.parentId != userId)
                    continue;

                if (other.name == name)
                {
                    if (!ui::messages::Resources::overrideLayoutTour(mainWindow()))
                        return;

                    layoutTourManager()->removeTour(other.id);
                    removeTourFromServer(other.id);
                }
            }

            tour.name = name;
            layoutTourManager()->addOrUpdateTour(tour);
            saveTourToServer(tour);
        });

    connect(action(action::RemoveLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            NX_EXPECT(!id.isNull());

            const auto tour = layoutTourManager()->tour(id);
            if (!tour.name.isEmpty())
            {
                QnSessionAwareMessageBox messageBox(mainWindow()   );
                messageBox.setIcon(QnMessageBoxIcon::Question);
                messageBox.setText(tr("Delete layout tour %1?").arg(tour.name));
                messageBox.setStandardButtons(QDialogButtonBox::Cancel);
                messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
                    QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
                if (messageBox.exec() == QDialogButtonBox::Cancel)
                    return;
            }

            layoutTourManager()->removeTour(id);
            removeTourFromServer(id);
        });

    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);

            if (!toggled)
            {
                NX_EXPECT(id.isNull());
                m_tourExecutor->stopCurrentTour();
            }

            if (toggled && id.isNull())
            {
                const auto reviewTourId = workbench()->currentLayout()->data(
                    Qn::LayoutTourUuidRole).value<QnUuid>();

                // Start Tour on a review layout must start the layout tour
                if (!reviewTourId.isNull())
                    id = reviewTourId;
            }

            if (id.isNull())
            {
                if (toggled)
                    m_tourExecutor->startSingleLayoutTour();
            }
            else
            {
                NX_EXPECT(toggled);
                m_tourExecutor->startTour(layoutTourManager()->tour(id));
                context()->instance<QnWorkbenchStateManager>()->saveState();
            }
            context()->action(action::FullscreenAction)->setChecked(true);
        });

    connect(action(action::PreviousLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleLayoutTourModeAction)->isChecked())
                m_tourExecutor->prevTourStep();
        });

    connect(action(action::NextLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleLayoutTourModeAction)->isChecked())
                m_tourExecutor->nextTourStep();
        });

    connect(action(action::SaveLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = layoutTourManager()->tour(id);
            if (!tour.isValid())
                return;
            saveTourToServer(tour);
        });

    connect(action(action::EscapeHotkeyAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleLayoutTourModeAction)->isChecked())
                action(action::ToggleLayoutTourModeAction)->toggle();

            // Just for safety
            NX_EXPECT(m_tourExecutor->runningTour().isNull());
            m_tourExecutor->stopCurrentTour();
        });
}

LayoutToursHandler::~LayoutToursHandler()
{
}

bool LayoutToursHandler::tryClose(bool /*force*/)
{
    m_tourExecutor->stopCurrentTour();
    return true;
}

void LayoutToursHandler::forcedUpdate()
{
    // Do nothing
}

void LayoutToursHandler::loadState(const QnWorkbenchState& state)
{
    if (!state.runningTourId.isNull())
    {
        menu()->trigger(action::ToggleLayoutTourModeAction, {Qn::UuidRole, state.runningTourId});
    }
}

void LayoutToursHandler::submitState(QnWorkbenchState* state)
{
    if (state)
        state->runningTourId = m_tourExecutor->runningTour();
}

void LayoutToursHandler::saveTourToServer(const ec2::ApiLayoutTourData& tour)
{
    NX_EXPECT(layoutTourManager()->tour(tour.id).isValid());
    auto stateManager = qnClientCoreModule->layoutTourStateManager();

    if (const auto connection = commonModule()->ec2Connection())
    {
        int reqId = connection->getLayoutTourManager(Qn::kSystemAccess)->save(tour, this,
            [tour, stateManager](int reqId, ec2::ErrorCode errorCode)
            {
                stateManager->removeSaveRequest(tour.id, reqId);
                if (stateManager->hasSaveRequests(tour.id))
                    return;

                stateManager->markBeingSaved(tour.id, false);
                if (errorCode == ec2::ErrorCode::ok)
                    stateManager->markChanged(tour.id, false);
            });
        const bool success = (reqId > 0);
        if (success)
        {
            stateManager->markBeingSaved(tour.id, true);
            stateManager->addSaveRequest(tour.id, reqId);
        }
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
