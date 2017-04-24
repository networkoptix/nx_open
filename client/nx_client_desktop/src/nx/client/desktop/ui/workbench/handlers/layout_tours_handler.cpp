#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <nx_ec/ec_api.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/style/skin.h>
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
    connect(qnLayoutTourManager, &QnLayoutTourManager::tourChanged, m_tourExecutor,
        &LayoutTourExecutor::updateTour);

    connect(qnLayoutTourManager, &QnLayoutTourManager::tourRemoved, m_tourExecutor,
        &LayoutTourExecutor::stopTour);

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
            menu()->trigger(QnActions::ReviewLayoutTourAction, {Qn::UuidRole, tour.id});
        });

    connect(action(QnActions::RenameLayoutTourAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
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
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            NX_EXPECT(!id.isNull());

            const auto tour = qnLayoutTourManager->tour(id);
            if (!tour.name.isEmpty())
            {
                //TODO: #GDM #3.1 add to table, fix text and buttons
                if (QnMessageBox::warning(
                    mainWindow(),
                    tr("Are you sure you want to delete %1?").arg(tour.name),
                    QString(),
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel) != QDialogButtonBox::Ok)
                {
                    return;
                }
            }

            qnLayoutTourManager->removeTour(id);
            removeTourFromServer(id);
        });

    connect(action(QnActions::ToggleLayoutTourModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);

            if (!toggled)
            {
                NX_EXPECT(id.isNull());
                m_tourExecutor->stopCurrentTour();
            }

            if (id.isNull())
            {
                if (toggled)
                    m_tourExecutor->startSingleLayoutTour();
            }
            else
            {
                NX_EXPECT(toggled);
                m_tourExecutor->startTour(qnLayoutTourManager->tour(id));
            }
        });

    connect(action(QnActions::SaveLayoutTourAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto tour = qnLayoutTourManager->tour(id);
            if (!tour.isValid())
                return;
            saveTourToServer(tour);
        });

    connect(action(QnActions::EscapeHotkeyAction), &QAction::triggered, this,
        [this]
        {
            if (action(QnActions::ToggleLayoutTourModeAction)->isChecked())
                action(QnActions::ToggleLayoutTourModeAction)->toggle();

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
        menu()->trigger(QnActions::ToggleLayoutTourModeAction, {Qn::UuidRole, state.runningTourId});
    }
}

void LayoutToursHandler::submitState(QnWorkbenchState* state)
{
    if (state)
        state->runningTourId = m_tourExecutor->runningTour();
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
