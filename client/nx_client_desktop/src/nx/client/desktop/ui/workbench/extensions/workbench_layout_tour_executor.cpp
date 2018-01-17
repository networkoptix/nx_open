#include "workbench_layout_tour_executor.h"

#include <QtCore/QTimerEvent>

#include <client/client_settings.h>

#include <core/resource/layout_resource.h>
#include <core/resource/layout_tour_item.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_layout_tour_data.h>

#include <nx/client/desktop/radass/radass_resource_manager.h>
#include <nx/client/desktop/radass/radass_types.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>

#include <utils/math/math.h>

namespace {

static const int kTimerPrecisionMs = 500;
static const int kHintTimeoutMs = 3000;

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutTourExecutor::LayoutTourExecutor(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
}

LayoutTourExecutor::~LayoutTourExecutor()
{
}

void LayoutTourExecutor::startTour(const ec2::ApiLayoutTourData& tour)
{
    if (m_mode != Mode::Stopped)
        stopCurrentTour();

    if (!tour.isValid())
        return;

    NX_EXPECT(m_tour.items.empty());
    resetTourItems(tour.items);
    if (tour.items.empty())
        return;

    m_mode = Mode::MultipleLayouts;
    m_tour.id = tour.id;
    m_tour.currentIndex = -1;
    clearWorkbenchState();

    startTourInternal();
    if (!tour.settings.manual)
        startTimer();
}

void LayoutTourExecutor::updateTour(const ec2::ApiLayoutTourData& tour)
{
    if (m_mode != Mode::MultipleLayouts)
        return;

    if (tour.id != m_tour.id)
        return;

    NX_EXPECT(tour.isValid());

    // Start/stop timer before items check.
    const bool isTourManual = !isTimerRunning();
    if (isTourManual != tour.settings.manual)
    {
        if (tour.settings.manual)
            stopTimer();
        else
            startTimer();
    }

    resetTourItems(tour.items);
    if (m_tour.items.empty())
        stopCurrentTour();
    else if (m_tour.currentIndex >= tour.items.size())
        m_tour.currentIndex = 0;
}

void LayoutTourExecutor::stopTour(const QnUuid& id)
{
    if (m_mode == Mode::MultipleLayouts && m_tour.id == id)
        stopCurrentTour();
}

void LayoutTourExecutor::startSingleLayoutTour()
{
    m_mode = Mode::SingleLayout;
    startTourInternal();
    startTimer();
}

QnUuid LayoutTourExecutor::runningTour() const
{
    if (m_mode == Mode::MultipleLayouts)
        return m_tour.id;
    return QnUuid();
}

void LayoutTourExecutor::prevTourStep()
{
    setHintVisible(false);
    processTourStepInternal(false, true);
}

void LayoutTourExecutor::nextTourStep()
{
    setHintVisible(false);
    processTourStepInternal(true, true);
}

void LayoutTourExecutor::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_tour.timerId)
        processTourStepInternal(true, false);
    base_type::timerEvent(event);
}

void LayoutTourExecutor::stopCurrentTour()
{
    // We can recursively get here from restoreWorkbenchState() call
    const auto mode = m_mode;
    m_mode = Mode::Stopped;

    switch (mode)
    {
        case Mode::SingleLayout:
        {
            stopTimer();
            setHintVisible(false);
            workbench()->setItem(Qn::ZoomedRole, nullptr);
            break;
        }

        case Mode::MultipleLayouts:
        {
            stopTimer();
            setHintVisible(false);
            m_tour.currentIndex = 0;
            NX_EXPECT(!m_tour.id.isNull());
            QnUuid tourId = m_tour.id;
            m_tour.id = QnUuid();
            resetTourItems({});
            restoreWorkbenchState(tourId);
            break;
        }

        default:
            break;
    }
}

void LayoutTourExecutor::resetTourItems(const ec2::ApiLayoutTourItemDataList& items)
{
    const auto radassManager = context()->instance<RadassResourceManager>();

    m_tour.items.clear();

    for (const auto& item: items)
    {
        auto existing = resourcePool()->getResourceById(item.resourceId);
        if (!existing)
            continue;

        const auto existingLayout = existing.dynamicCast<QnLayoutResource>();

        QHash<QnUuid, QnUuid> remapHash;

        QnLayoutResourcePtr layout = existingLayout
            ? existingLayout->clone(&remapHash)
            : QnLayoutResource::createFromResource(existing);

        NX_EXPECT(layout);
        if (!layout)
            continue;

        if (existingLayout)
        {
            for (auto iter = remapHash.cbegin(); iter != remapHash.cend(); ++iter)
            {
                const QnLayoutItemIndex oldIndex(existingLayout, iter.key());
                const QnLayoutItemIndex newIndex(layout, iter.value());
                const auto mode = radassManager->mode(oldIndex);
                if (mode != RadassMode::Auto)
                    radassManager->setMode(newIndex, mode);
            }
        }

        layout->addFlags(Qn::local);
        layout->setData(Qn::LayoutFlagsRole, qVariantFromValue(QnLayoutFlag::FixedViewport
            | QnLayoutFlag::NoResize
            | QnLayoutFlag::NoMove
            | QnLayoutFlag::NoTimeline
            | QnLayoutFlag::FillViewport
        ));
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));

        m_tour.items.push_back(QnLayoutTourItem(layout, item.delayMs));
    }
}

void LayoutTourExecutor::processTourStepInternal(bool forward, bool force)
{
    auto nextIndex = [forward](int current, int size)
        {
            NX_EXPECT(size > 0);
            if (current < 0 || size <= 0)
                return 0;

            // adding size to avoid negative modulo result
            return (current + size + (forward ? 1 : -1)) % size;
        };

    switch (m_mode)
    {
        case Mode::SingleLayout:
        {
            const auto item = workbench()->item(Qn::ZoomedRole);
            if (!force
                && item
                && isTimerRunning()
                && !m_tour.elapsed.hasExpired(qnSettings->tourCycleTime()))
            {
                return;
            }

            auto items = workbench()->currentLayout()->items().toList();
            if (items.empty())
            {
                stopCurrentTour();
                return;
            }

            QnWorkbenchItem::sortByGeometry(&items);
            const int index = item ? items.indexOf(item) : -1;
            workbench()->setItem(Qn::ZoomedRole, items[nextIndex(index, items.size())]);
            if (isTimerRunning())
                m_tour.elapsed.restart();
            break;
        }
        case Mode::MultipleLayouts:
        {
            NX_EXPECT(!m_tour.id.isNull());
            const bool hasItems = !m_tour.items.empty();
            NX_EXPECT(hasItems);
            if (!hasItems)
            {
                stopCurrentTour();
                return;
            }

            const bool isRunning = isTimerRunning()
                && qBetween(0, m_tour.currentIndex, (int) m_tour.items.size());

            if (isRunning && !force)
            {
                // No need to switch the only item.
                if (m_tour.items.size() < 2)
                    return;

                const auto& item = m_tour.items[m_tour.currentIndex];
                if (!m_tour.elapsed.hasExpired(item.delayMs))
                    return;
            }

            m_tour.currentIndex = nextIndex(m_tour.currentIndex, (int) m_tour.items.size());
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
            if (isTimerRunning())
                m_tour.elapsed.restart();
            workbench()->setCurrentLayout(wbLayout);
            break;
        }
        default:
            break;
    }
}

void LayoutTourExecutor::clearWorkbenchState()
{
    m_lastState = QnWorkbenchState();
    workbench()->submit(m_lastState);
    workbench()->clear();
}

void LayoutTourExecutor::restoreWorkbenchState(const QnUuid& tourId)
{
    workbench()->clear();

    if (m_lastState.layoutUuids.isEmpty() && !tourId.isNull())
    {
        m_lastState.layoutUuids.push_back(tourId);
        m_lastState.currentLayoutId = tourId;
    }
    workbench()->update(m_lastState);

    const bool validState = !workbench()->layouts().empty()
        && workbench()->currentLayout()->resource();
    NX_EXPECT(validState);
    if (!validState)
        menu()->trigger(action::OpenNewTabAction);
}

void LayoutTourExecutor::setHintVisible(bool visible)
{
    if (visible)
    {
        const auto hint = m_mode == Mode::MultipleLayouts
            ? tr("Use keyboard arrows to switch layouts. To exit the showreel press Esc.")
            : tr("Press any key to stop the tour.");

        m_hintLabel = QnGraphicsMessageBox::information(hint, kHintTimeoutMs);
    }
    else if (m_hintLabel)
    {
        m_hintLabel->hideImmideately();
        m_hintLabel.clear();
    }
}

void LayoutTourExecutor::startTimer()
{
    NX_EXPECT(m_tour.timerId == 0);
    if (m_tour.timerId == 0)
        m_tour.timerId = QObject::startTimer(kTimerPrecisionMs);
    NX_EXPECT(!m_tour.elapsed.isValid());
    m_tour.elapsed.start();
}

void LayoutTourExecutor::stopTimer()
{
    if (!isTimerRunning())
        return;

    NX_EXPECT(m_tour.timerId != 0);
    killTimer(m_tour.timerId);
    m_tour.timerId = 0;
    NX_EXPECT(m_tour.elapsed.isValid());
    m_tour.elapsed.invalidate();
}

void LayoutTourExecutor::startTourInternal()
{
    setHintVisible(true);
    processTourStepInternal(true, true);
}

bool LayoutTourExecutor::isTimerRunning() const
{
    return m_tour.elapsed.isValid();
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
