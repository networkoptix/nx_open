// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_executor.h"

#include <QtCore/QTimerEvent>

#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/math.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

namespace {

using namespace std::chrono;

static constexpr auto kTimerPrecision = 500ms;
static constexpr auto kHintTimeout = 3s;

} // namespace

namespace nx::vms::client::desktop {

ShowreelExecutor::ShowreelExecutor(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
}

ShowreelExecutor::~ShowreelExecutor()
{
}

void ShowreelExecutor::startShowreel(const nx::vms::api::ShowreelData& showreel)
{
    if (m_mode != Mode::stopped)
        stopCurrentShowreel();

    if (!showreel.isValid())
        return;

    NX_ASSERT(m_showreel.items.empty());
    resetShowreelItems(showreel.items);
    if (showreel.items.empty())
        return;

    m_mode = Mode::multipleLayouts;
    m_showreel.id = showreel.id;
    m_showreel.currentIndex = -1;

    // Here we saving actual workbench state, which will be restored after the showreel stop.
    clearWorkbenchState();

    startShowreelInternal();
    if (!showreel.settings.manual)
        startTimer();

    appContext()->clientStateHandler()->storeSystemSpecificState();
}

void ShowreelExecutor::updateShowreel(const nx::vms::api::ShowreelData& showreel)
{
    if (m_mode != Mode::multipleLayouts)
        return;

    if (showreel.id != m_showreel.id)
        return;

    NX_ASSERT(showreel.isValid());

    // Start/stop timer before items check.
    const bool isShowreelManual = !isTimerRunning();
    if (isShowreelManual != showreel.settings.manual)
    {
        if (showreel.settings.manual)
            stopTimer();
        else
            startTimer();
    }

    resetShowreelItems(showreel.items);
    if (m_showreel.items.empty())
        stopCurrentShowreel();
    else if (m_showreel.currentIndex >= showreel.items.size())
        m_showreel.currentIndex = 0;
}

void ShowreelExecutor::stopShowreel(const QnUuid& id)
{
    if (m_mode == Mode::multipleLayouts && m_showreel.id == id)
    {
        stopCurrentShowreel();
        appContext()->clientStateHandler()->storeSystemSpecificState();
    }
}

void ShowreelExecutor::startSingleLayoutShowreel()
{
    m_mode = Mode::singleLayout;
    startShowreelInternal();
    startTimer();
}

QnUuid ShowreelExecutor::runningShowreel() const
{
    if (m_mode == Mode::multipleLayouts)
        return m_showreel.id;
    return QnUuid();
}

void ShowreelExecutor::prevShowreelStep()
{
    setHintVisible(false);
    processShowreelStepInternal(/*forward*/ false, /*force*/ true);
}

void ShowreelExecutor::nextShowreelStep()
{
    setHintVisible(false);
    processShowreelStepInternal(/*forward*/ true, /*force*/ true);
}

void ShowreelExecutor::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_showreel.timerId)
        processShowreelStepInternal(/*forward*/ true, /*force*/ false);
    base_type::timerEvent(event);
}

void ShowreelExecutor::stopCurrentShowreel()
{
    // We can recursively get here from restoreWorkbenchState() call
    const auto mode = m_mode;
    m_mode = Mode::stopped;

    switch (mode)
    {
        case Mode::singleLayout:
        {
            stopTimer();
            setHintVisible(false);
            workbench()->setItem(Qn::ZoomedRole, nullptr);
            break;
        }

        case Mode::multipleLayouts:
        {
            stopTimer();
            setHintVisible(false);
            m_showreel.currentIndex = 0;
            NX_ASSERT(!m_showreel.id.isNull());
            const QnUuid showreelId = m_showreel.id;
            m_showreel.id = QnUuid();
            // We don't want to restart Showreel cyclically.
            m_lastState.runningTourId = QnUuid();
            resetShowreelItems({});
            restoreWorkbenchState(showreelId);
            break;
        }

        default:
            break;
    }
}

void ShowreelExecutor::suspendCurrentShowreel()
{
    if (isTimerRunning())
        stopTimer();
}

void ShowreelExecutor::resumeCurrentShowreel()
{
    if (!isTimerRunning())
        startTimer();
}

void ShowreelExecutor::resetShowreelItems(const nx::vms::api::ShowreelItemDataList& items)
{
    const auto radassManager = context()->instance<RadassResourceManager>();

    m_showreel.items.clear();

    for (const auto& item: items)
    {
        auto existing = resourcePool()->getResourceById(item.resourceId);
        if (!existing)
            continue;

        const auto existingLayout = existing.dynamicCast<LayoutResource>();

        LayoutResource::ItemsRemapHash remapHash;

        LayoutResourcePtr layout = existingLayout
            ? existingLayout->clone(&remapHash)
            : layoutFromResource(existing);

        if (!NX_ASSERT(layout))
            continue;

        if (existingLayout)
        {
            for (auto iter = remapHash.cbegin(); iter != remapHash.cend(); ++iter)
            {
                const LayoutItemIndex oldIndex(existingLayout, iter.key());
                const LayoutItemIndex newIndex(layout, iter.value());
                const auto mode = radassManager->mode(oldIndex);
                if (mode != RadassMode::Auto)
                    radassManager->setMode(newIndex, mode);
            }
        }

        layout->addFlags(Qn::local);
        layout->setData(Qn::LayoutFlagsRole, QVariant::fromValue(QnLayoutFlag::FixedViewport
            | QnLayoutFlag::NoResize
            | QnLayoutFlag::NoMove
            | QnLayoutFlag::NoTimeline
            | QnLayoutFlag::FillViewport
        ));
        layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));

        m_showreel.items.push_back(Item{layout, item.delayMs});
    }
}

void ShowreelExecutor::processShowreelStepInternal(bool forward, bool force)
{
    auto nextIndex =
        [forward](int current, int size)
        {
            NX_ASSERT(size > 0);
            if (current < 0 || size <= 0)
                return 0;

            // adding size to avoid negative modulo result
            return (current + size + (forward ? 1 : -1)) % size;
        };

    switch (m_mode)
    {
        case Mode::singleLayout:
        {
            const auto item = workbench()->item(Qn::ZoomedRole);
            if (!force
                && item
                && isTimerRunning()
                && !m_showreel.elapsed.hasExpired(appContext()->localSettings()->tourCycleTimeMs()))
            {
                return;
            }

            auto items = workbench()->currentLayout()->items().values();
            if (items.empty())
            {
                stopCurrentShowreel();
                return;
            }

            QnWorkbenchItem::sortByGeometryAndName(items);
            const int index = item ? items.indexOf(item) : -1;
            workbench()->setItem(Qn::ZoomedRole, items[nextIndex(index, items.size())]);
            if (isTimerRunning())
                m_showreel.elapsed.restart();
            break;
        }
        case Mode::multipleLayouts:
        {
            NX_ASSERT(!m_showreel.id.isNull());
            const bool hasItems = !m_showreel.items.empty();
            NX_ASSERT(hasItems);
            if (!hasItems)
            {
                stopCurrentShowreel();
                return;
            }

            const bool isRunning = isTimerRunning()
                && qBetween(0, m_showreel.currentIndex, (int) m_showreel.items.size());

            if (isRunning && !force)
            {
                // No need to switch the only item.
                if (m_showreel.items.size() < 2)
                    return;

                const auto& item = m_showreel.items[m_showreel.currentIndex];
                if (!m_showreel.elapsed.hasExpired(item.delayMs))
                    return;
            }

            m_showreel.currentIndex = nextIndex(
                m_showreel.currentIndex,
                (int) m_showreel.items.size());
            const auto& next = m_showreel.items[m_showreel.currentIndex];

            auto layout = next.layout;
            if (!layout)
            {
                stopCurrentShowreel();
                return;
            }

            auto wbLayout = workbench()->layout(layout);
            if (!wbLayout)
                wbLayout = workbench()->addLayout(layout);

            if (isTimerRunning())
                m_showreel.elapsed.restart();
            workbench()->setCurrentLayout(wbLayout);
            break;
        }
        default:
            break;
    }
}

void ShowreelExecutor::clearWorkbenchState()
{
    m_lastState = WorkbenchState();
    workbench()->submit(m_lastState);
    workbench()->clear();
}

void ShowreelExecutor::restoreWorkbenchState(const QnUuid& tourId)
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
    if (!NX_ASSERT(validState))
        menu()->trigger(menu::OpenNewTabAction);
}

void ShowreelExecutor::setHintVisible(bool visible)
{
    if (visible)
    {
        const auto hint = m_mode == Mode::multipleLayouts
            ? tr("Use keyboard arrows to switch layouts. To exit the showreel press Esc.")
            : tr("Press Esc to stop the tour.");

        m_hintLabel = SceneBanner::show(hint, kHintTimeout);
    }
    else if (m_hintLabel)
    {
        m_hintLabel->hide(/*immediately*/ true);
        m_hintLabel.clear();
    }
}

void ShowreelExecutor::startTimer()
{
    NX_ASSERT(m_showreel.timerId == 0);
    if (m_showreel.timerId == 0)
        m_showreel.timerId = QObject::startTimer(kTimerPrecision);
    NX_ASSERT(!m_showreel.elapsed.isValid());
    m_showreel.elapsed.start();
}

void ShowreelExecutor::stopTimer()
{
    if (!isTimerRunning())
        return;

    NX_ASSERT(m_showreel.timerId != 0);
    killTimer(m_showreel.timerId);
    m_showreel.timerId = 0;
    NX_ASSERT(m_showreel.elapsed.isValid());
    m_showreel.elapsed.invalidate();
}

void ShowreelExecutor::startShowreelInternal()
{
    setHintVisible(true);
    processShowreelStepInternal(/*forward*/ true, /*force*/ true);
}

bool ShowreelExecutor::isTimerRunning() const
{
    return m_showreel.elapsed.isValid();
}

} // namespace nx::vms::client::desktop
