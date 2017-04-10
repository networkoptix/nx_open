#include "workbench_layout_tour_controller.h"

#include <QtCore/QTimerEvent>

#include <core/resource/layout_tour_item.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_tour_data.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>

#include <ui/actions/action_manager.h> //TODO: #GDM #3.1 think about deps inversion

#include <utils/math/math.h>


namespace {

static const int kTimerPrecisionMs = 500;

}

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutTourController::LayoutTourController(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{

}

LayoutTourController::~LayoutTourController()
{

}

void LayoutTourController::startTour(const ec2::ApiLayoutTourData& tour)
{
    if (m_mode != Mode::Stopped)
        stopTourInternal();

    if (!tour.isValid())
        return;

    const auto items = QnLayoutTourItem::createList(tour.items, qnResPool);
    if (items.empty())
        return;

    m_mode = Mode::MultipleLayouts;
    m_tour.id = tour.id;
    m_tour.items = std::move(items);
    clearWorkbenchState();

    QnWorkbenchLayout* firstLayout = nullptr;
    for (const auto& item: m_tour.items)
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
    NX_EXPECT(m_tour.timerId == 0);
    if (m_tour.timerId == 0)
        m_tour.timerId = startTimer(kTimerPrecisionMs);
    NX_EXPECT(m_tour.currentIndex == 0);
    NX_EXPECT(!m_tour.elapsed.isValid());
    m_tour.elapsed.start();
    workbench()->setCurrentLayout(firstLayout);

    //action(QnActions::EffectiveMaximizeAction)->setChecked(false);
    menu()->trigger(QnActions::FreespaceAction);
}

void LayoutTourController::updateTour(const ec2::ApiLayoutTourData& tour)
{
    if (m_mode != Mode::MultipleLayouts)
        return;

    if (tour.id != m_tour.id)
        return;

    NX_EXPECT(tour.isValid());
    m_tour.items = QnLayoutTourItem::createList(tour.items, qnResPool);
    if (m_tour.items.empty())
        stopTourInternal();
    else if (m_tour.currentIndex >= tour.items.size())
        m_tour.currentIndex = 0;
}

void LayoutTourController::stopTour(const QnUuid& id)
{
    if (m_mode == Mode::MultipleLayouts && m_tour.id == id)
        stopTourInternal();
}

QnUuid LayoutTourController::runningTour() const
{
    if (m_mode == Mode::MultipleLayouts)
        return m_tour.id;
    return QnUuid();
}

void LayoutTourController::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_tour.timerId)
        processTourStep();
    base_type::timerEvent(event);
}

void LayoutTourController::stopTourInternal()
{
    switch (m_mode)
    {
        case Mode::MultipleLayouts:
        {
            NX_EXPECT(m_tour.timerId != 0);
            killTimer(m_tour.timerId);

            NX_EXPECT(m_tour.elapsed.isValid());
            m_tour.elapsed.invalidate();

            m_tour.currentIndex = 0;
            NX_EXPECT(!m_tour.id.isNull());
            m_tour.id = QnUuid();
            m_tour.items.clear();

            restoreWorkbenchState();
            break;
        }

        default:
            break;
    }
}

void LayoutTourController::processTourStep()
{
    NX_EXPECT(m_mode == Mode::MultipleLayouts);
    NX_EXPECT(!m_tour.id.isNull());
    NX_EXPECT(m_tour.items.size() > 0);
    NX_EXPECT(m_tour.elapsed.isValid());

    const bool hasItem = qBetween(0, m_tour.currentIndex, (int)m_tour.items.size());
    NX_EXPECT(hasItem);
    if (!hasItem)
    {
        stopTourInternal();
        return;
    }

    // No need to switch the only item.
    if (m_tour.items.size() < 2)
        return;

    const auto& item = m_tour.items[m_tour.currentIndex];
    if (!m_tour.elapsed.hasExpired(item.delayMs))
        return;

    m_tour.currentIndex = (m_tour.currentIndex + 1) % m_tour.items.size();
    const auto& next = m_tour.items[m_tour.currentIndex];

    auto layout = next.layout;
    if (!layout)
    {
        stopTourInternal();
        return;
    }

    auto wbLayout = QnWorkbenchLayout::instance(layout);
    if (!wbLayout)
    {
        wbLayout = qnWorkbenchLayoutsFactory->create(layout, workbench());
        workbench()->addLayout(wbLayout);
    }
    m_tour.elapsed.restart();
    workbench()->setCurrentLayout(wbLayout);
}

void LayoutTourController::clearWorkbenchState()
{
    m_lastState = QnWorkbenchState();
    workbench()->submit(m_lastState);
    workbench()->clear();
}

void LayoutTourController::restoreWorkbenchState()
{
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
