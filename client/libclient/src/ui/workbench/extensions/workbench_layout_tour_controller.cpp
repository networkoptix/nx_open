#include "workbench_layout_tour_controller.h"

#include <QtCore/QTimerEvent>

#include <client/client_settings.h>

#include <core/resource/layout_tour_item.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_tour_data.h>

#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/client/ui/workbench/layouts/layout_factory.h>

#include <ui/actions/action_manager.h>

#include <utils/math/math.h>


namespace {

static const int kTimerPrecisionMs = 500;


} // namespace

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
        stopCurrentTour();

    if (!tour.isValid())
        return;

    const auto items = QnLayoutTourItem::createList(tour.items, resourcePool());
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
    startTimer();
    workbench()->setCurrentLayout(firstLayout);
    NX_EXPECT(m_tour.currentIndex == 0);

    //action(QnActions::EffectiveMaximizeAction)->setChecked(false);
    menu()->trigger(QnActions::FreespaceAction);
    setHintVisible(true);
}

void LayoutTourController::updateTour(const ec2::ApiLayoutTourData& tour)
{
    if (m_mode != Mode::MultipleLayouts)
        return;

    if (tour.id != m_tour.id)
        return;

    NX_EXPECT(tour.isValid());
    m_tour.items = QnLayoutTourItem::createList(tour.items, resourcePool());
    if (m_tour.items.empty())
        stopCurrentTour();
    else if (m_tour.currentIndex >= tour.items.size())
        m_tour.currentIndex = 0;
}

void LayoutTourController::stopTour(const QnUuid& id)
{
    if (m_mode == Mode::MultipleLayouts && m_tour.id == id)
        stopCurrentTour();
}

void LayoutTourController::toggleLayoutTour(bool start)
{
    // Stop layouts tour if running
    if (!start || m_mode == Mode::MultipleLayouts)
        stopCurrentTour();

    if (start)
    {
        m_mode = Mode::SingleLayout;
        startTimer();
        setHintVisible(true);
        processTourStep();
    }

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

void LayoutTourController::stopCurrentTour()
{
    switch (m_mode)
    {
        case Mode::SingleLayout:
        {
            stopTimer();
            workbench()->setItem(Qn::ZoomedRole, NULL);

            break;
        }

        case Mode::MultipleLayouts:
        {
            stopTimer();
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

    setHintVisible(false);
    m_mode = Mode::Stopped;
}

void LayoutTourController::processTourStep()
{
    NX_EXPECT(m_tour.elapsed.isValid());
    switch (m_mode)
    {
        case Mode::SingleLayout:
        {
            auto item = workbench()->item(Qn::ZoomedRole);
            if (item && !m_tour.elapsed.hasExpired(qnSettings->tourCycleTime()))
                return;

            auto items = workbench()->currentLayout()->items().toList();
            std::sort(items.begin(), items.end(),
                [](QnWorkbenchItem *l, QnWorkbenchItem *r)
                {
                    QRect lg = l->geometry();
                    QRect rg = r->geometry();
                    return lg.y() < rg.y() || (lg.y() == rg.y() && lg.x() < rg.x());
                });

            if (items.empty())
            {
                stopCurrentTour();
                return;
            }

            if (item)
                item = items[(items.indexOf(item) + 1) % items.size()];
            else
                item = items[0];
            workbench()->setItem(Qn::ZoomedRole, item);
            m_tour.elapsed.restart();
            break;
        }
        case Mode::MultipleLayouts:
        {
            NX_EXPECT(!m_tour.id.isNull());
            NX_EXPECT(m_tour.items.size() > 0);
            const bool hasItem = qBetween(0, m_tour.currentIndex, (int)m_tour.items.size());
            NX_EXPECT(hasItem);
            if (!hasItem)
            {
                stopCurrentTour();
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
                stopCurrentTour();
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
            break;
        }
        default:
        {
            NX_EXPECT(false);
            stopCurrentTour();
            break;
        }
    }
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

void LayoutTourController::setHintVisible(bool visible)
{
    if (visible)
    {
        m_hintLabel = QnGraphicsMessageBox::information(
            tr("Press any key to stop the tour."));
    }
    else if (m_hintLabel)
    {
        m_hintLabel->hideImmideately();
        m_hintLabel.clear();
    }
}

void LayoutTourController::startTimer()
{
    NX_EXPECT(m_tour.timerId == 0);
    if (m_tour.timerId == 0)
        m_tour.timerId = QObject::startTimer(kTimerPrecisionMs);
    NX_EXPECT(!m_tour.elapsed.isValid());
    m_tour.elapsed.start();
}

void LayoutTourController::stopTimer()
{
    NX_EXPECT(m_tour.timerId != 0);
    killTimer(m_tour.timerId);
    m_tour.timerId = 0;
    NX_EXPECT(m_tour.elapsed.isValid());
    m_tour.elapsed.invalidate();
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
