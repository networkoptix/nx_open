#include "layout_tours_handler.h"

#include <QtWidgets/QAction>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/layout_resource.h>

#include <ui/actions/action_manager.h>
#include <ui/dialogs/layout_tour_dialog.h>
#include <ui/workbench/workbench.h>
#include <ui/style/skin.h>

#include <ui/workbench/workbench_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>

#include <nx/utils/string.h>

#include <utils/math/math.h>

namespace {

static const int kTimerPrecisionMs = 500;

}

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutToursHandler::LayoutToursHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnLayoutTourManager, &QnLayoutTourManager::tourChanged, this,
        [this](const ec2::ApiLayoutTourData& tour)
        {
            if (tour.id == m_runningTourId)
                stopTour();
        });

    connect(qnLayoutTourManager, &QnLayoutTourManager::tourRemoved, this,
        [this](const ec2::ApiLayoutTourData& tour)
        {
            if (tour.id == m_runningTourId)
                stopTour();
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

            startTour(tour);
        });

    connect(action(QnActions::StopLayoutTourAction), &QAction::triggered, this,
        &LayoutToursHandler::stopTour);

    connect(action(QnActions::OpenLayoutTourAction), &QAction::triggered,
        this, &LayoutToursHandler::openToursLayout);
}

void LayoutToursHandler::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_timerId)
        base_type::timerEvent(event);

    processTourStep();
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
    resourcePool()->addResource(resource);
    menu()->trigger(QnActions::OpenSingleLayoutAction, resource);
}

void LayoutToursHandler::startTour(const ec2::ApiLayoutTourData& tour)
{
    const auto items = qnLayoutTourManager->tourItems(tour);
    if (items.empty())
        return;

    m_runningTourId = tour.id;
    m_lastState = QnWorkbenchState();
    workbench()->submit(m_lastState);
    workbench()->clear();

    QnWorkbenchLayout* firstLayout = nullptr;
    //TODO #GDM #3.1 move logic to a separate controller
    for (const auto& item: items)
    {
        auto layout = QnWorkbenchLayout::instance(item.layout);
        if (!layout)
        {
            layout = qnWorkbenchLayoutsFactory->create(item.layout, workbench());
            workbench()->addLayout(layout);
        }
        if (!firstLayout)
            firstLayout = layout;
    }
    if (m_timerId == 0)
        m_timerId = startTimer(kTimerPrecisionMs);
    m_currentIndex = 0;
    m_elapsed.start();
    workbench()->setCurrentLayout(firstLayout);

    //action(QnActions::EffectiveMaximizeAction)->setChecked(false);
    menu()->trigger(QnActions::FreespaceAction);
}

void LayoutToursHandler::processTourStep()
{
    auto tour = qnLayoutTourManager->tour(m_runningTourId);
    NX_ASSERT(tour.isValid());
    if (!tour.isValid())
    {
        stopTour();
        return;
    }

    const bool hasItem = qBetween(0, m_currentIndex, (int)tour.items.size());
    NX_ASSERT(hasItem);
    if (!hasItem)
    {
        stopTour();
        return;
    }

    // No need to switch the only item.
    if (tour.items.size() < 2)
        return;

    NX_ASSERT(m_elapsed.isValid());
    if (!m_elapsed.isValid())
        m_elapsed.start();

    const auto& item = tour.items[m_currentIndex];
    if (!m_elapsed.hasExpired(item.delayMs))
        return;

    m_currentIndex = (m_currentIndex + 1) % tour.items.size();
    const auto& next = tour.items[m_currentIndex];

    auto layout = resourcePool()->getResourceById<QnLayoutResource>(next.layoutId);
    if (!layout)
    {
        stopTour();
        return;
    }

    auto wbLayout = QnWorkbenchLayout::instance(layout);
    if (!wbLayout)
    {
        wbLayout = qnWorkbenchLayoutsFactory->create(layout, workbench());
        workbench()->addLayout(wbLayout);
    }
    m_elapsed.restart();
    workbench()->setCurrentLayout(wbLayout);
}

void LayoutToursHandler::stopTour()
{
    if (m_timerId != 0)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    m_elapsed.invalidate();
    m_currentIndex = 0;
    m_runningTourId = QnUuid();

    workbench()->clear();
    workbench()->update(m_lastState);

    if (workbench()->layouts().empty() || !workbench()->currentLayout()->resource())
        menu()->trigger(QnActions::OpenNewTabAction);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
