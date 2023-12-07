// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_navigator.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QMenu>

#include <camera/cam_display.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_data_manager.h>
#include <camera/client_video_camera.h>
#include <camera/resource_display.h>
#include <client/client_runtime_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/datetime.h>
#include <nx/utils/math/math.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/scene/widgets/timeline_calendar_widget.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/state/thumbnail_search_state.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_loading_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <server/server_storage_manager.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/widgets/main_window.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "nx/vms/client/desktop/menu/action_conditions.h"
#include "nx/vms/client/desktop/menu/actions.h"
#include "watchers/workbench_user_inactivity_watcher.h"
#include "workbench_context.h"
#include "workbench_display.h"
#include "workbench_item.h"
#include "workbench_layout.h"

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace std::chrono;

namespace {

const int kCameraHistoryRetryTimeoutMs = 5 * 1000;

/** Size of timeline window near live when there is no recorded periods on cameras. */
const int kTimelineWindowNearLive = 10 * 1000;

const int kMaxTimelineCameraNameLength = 30;

const int kUpdateBookmarksInterval = 2000;

const qreal kCatchUpTimeFactor = 30.0;

const qint64 kCatchUpThresholdMs = 500;
const qint64 kAdvanceIntervalMs = 200;

enum { kMinimalSymbolsCount = 3, kDelayMs = 750 };

QAtomicInt qn_threadedMergeHandle(1);

constexpr std::chrono::milliseconds kShiftStepMs = 10s;

bool isLivePosition(const QPointer<QnResourceWidget> widget)
{
    const auto time = widget->item()->data<qint64>(Qn::ItemTimeRole, -1);
    return time == -1 || time == DATETIME_NOW;
}

QnArchiveStreamReader* getReader(QnResourceWidget* widget)
{
    auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
    if (!mediaWidget)
        return nullptr;

    auto reader = mediaWidget->display()->archiveReader();
    return dynamic_cast<QnArchiveStreamReader*>(reader);
}

bool resourceIsVmax(const QnMediaResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnSecurityCamResource>();
    return camera && camera->isDtsBased() && camera->licenseType() == Qn::LC_VMAX;
}

qreal cachedSpeed(const QnMediaResourceWidget* widget)
{
    if (!widget)
        return 0.0;

    const auto targetWidget = widget->isZoomWindow() ? widget->zoomTargetWidget() : widget;
    return targetWidget->item()->data(Qn::ItemSpeedRole).toReal();
}

void cacheSpeed(qreal speed, QnMediaResourceWidget* widget)
{
    if (!widget)
        return;

    const auto targetWidget = widget->isZoomWindow() ? widget->zoomTargetWidget() : widget;
    targetWidget->item()->setData(Qn::ItemSpeedRole, speed);
}

} //namespace

QnWorkbenchNavigator::QnWorkbenchNavigator(WindowContext* context):
    base_type(context),
    WindowContextAware(context),
    m_lastSpeedRange(0.0, 0.0),
    m_startSelectionAction(new QAction(this)),
    m_endSelectionAction(new QAction(this)),
    m_clearSelectionAction(new QAction(this)),
    m_sliderBookmarksRefreshOperation(
        new nx::utils::PendingOperation(
            [this]{ updateSliderBookmarks(); },
            kUpdateBookmarksInterval,
            this))
{
    m_updateSliderTimer.restart();

    connect(appContext(), &ApplicationContext::systemContextAdded, this,
        &QnWorkbenchNavigator::connectToContext);
    for (auto systemContext: appContext()->systemContexts())
        connectToContext(systemContext);

    connect(this, &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnWorkbenchNavigator::updateTimelineRelevancy);
    connect(this, &QnWorkbenchNavigator::isRecordingChanged, this,
        &QnWorkbenchNavigator::updateTimelineRelevancy);
    connect(this, &QnWorkbenchNavigator::hasArchiveChanged, this,
        &QnWorkbenchNavigator::updateTimelineRelevancy);

    connect(system()->resourcePool(), &QnResourcePool::statusChanged, this,
        [this](const QnResourcePtr& resource)
        {
            auto cam = resource.dynamicCast<QnSecurityCamResource>();
            if (!m_syncedResources.contains(cam))
                return;

            if (!m_isRecording && resource->getStatus() == nx::vms::api::ResourceStatus::recording)
                updateIsRecording(true);
            else
                updateFootageState();
        });

    connect(system()->cameraHistoryPool(), &QnCameraHistoryPool::cameraHistoryInvalidated, this,
        [this](const QnSecurityCamResourcePtr &camera)
        {
            if (hasWidgetWithCamera(camera))
                updateHistoryForCamera(camera);
        });

    for (int i = 0; i < Qn::TimePeriodContentCount; ++i)
    {
        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        if (!ini().enableSyncedChunksForExtraContent && timePeriodType != Qn::RecordingContent)
            continue;

        auto chunksMergeTool = new QnThreadedChunksMergeTool();
        m_threadedChunksMergeTool[timePeriodType].reset(chunksMergeTool);

        connect(chunksMergeTool, &QnThreadedChunksMergeTool::finished, this,
            [this, timePeriodType](int handle, const QnTimePeriodList& result)
            {
                if (handle != m_chunkMergingProcessHandle)
                    return;

                NX_VERBOSE(this, "Merging of %1 is finished with %2 chunks",
                    timePeriodType, result.size());
                m_mergedTimePeriods[timePeriodType] = result;

                if (timePeriodType != Qn::RecordingContent && timePeriodType == selectedExtraContent())
                    updatePlaybackMask();

                if (m_timeSlider)
                    m_timeSlider->setTimePeriods(SyncedLine, timePeriodType, result);
                if (m_calendar)
                    m_calendar->setAllCamerasTimePeriods(timePeriodType, result);
            });

        chunksMergeTool->start();
    }

    QTimer* updateCameraHistoryTimer = new QTimer(this);
    updateCameraHistoryTimer->setInterval(kCameraHistoryRetryTimeoutMs);
    updateCameraHistoryTimer->setSingleShot(false);
    connect(updateCameraHistoryTimer, &QTimer::timeout, this, [this]
    {
        QSet<QnSecurityCamResourcePtr> localQueue = m_updateHistoryQueue;
        for (const QnSecurityCamResourcePtr &camera : localQueue)
            if (hasWidgetWithCamera(camera))
                updateHistoryForCamera(camera);
        m_updateHistoryQueue.clear();
    });
    updateCameraHistoryTimer->start();

    connect(workbench(), &Workbench::currentLayoutAboutToBeChanged, this,
        [this]
        {
            m_chunkMergingProcessHandle = qn_threadedMergeHandle.fetchAndAddAcquire(1);
            NX_VERBOSE(this, "Updated chunk merging handle to %1", m_chunkMergingProcessHandle);
        });

    connect(workbench(), &Workbench::currentLayoutItemRemoved, this,
        &QnWorkbenchNavigator::currentLayoutItemRemoved, Qt::QueuedConnection);

    connect(workbench(), &Workbench::currentLayoutChanged, this,
        [this]
        {
            resetSyncedPeriods();
            updateSyncedPeriods();
        });
}

QnWorkbenchNavigator::~QnWorkbenchNavigator()
{
}

QnTimeSlider *QnWorkbenchNavigator::timeSlider() const
{
    return m_timeSlider;
}

void QnWorkbenchNavigator::setTimeSlider(QnTimeSlider *timeSlider)
{
    if (m_timeSlider == timeSlider)
        return;

    if (m_timeSlider)
    {
        m_timeSlider->disconnect(this);

        if (isValid())
            deinitialize();
    }

    m_timeSlider = timeSlider;

    if (m_timeSlider)
    {
        connect(m_timeSlider, &QObject::destroyed, this, [this]() { setTimeSlider(nullptr); });

        if (isValid())
            initialize();
    }
}

QnTimeScrollBar *QnWorkbenchNavigator::timeScrollBar() const
{
    return m_timeScrollBar;
}

void QnWorkbenchNavigator::setTimeScrollBar(QnTimeScrollBar *scrollBar)
{
    if (m_timeScrollBar == scrollBar)
        return;

    if (m_timeScrollBar)
    {
        m_timeScrollBar->disconnect(this);

        if (isValid())
            deinitialize();
    }

    m_timeScrollBar = scrollBar;

    if (m_timeScrollBar)
    {
        connect(m_timeScrollBar, &QObject::destroyed, this,
            [this]() { setTimeScrollBar(nullptr); });

        connect(m_timeScrollBar, &QnTimeScrollBar::actionTriggered, this,
            [this](int action)
            {
                // Flag is required to avoid unzoom when moving slider by clicking outside of it.
                if (action != AbstractGraphicsSlider::SliderMove)
                    m_ignoreScrollBarDblClick = true;
            });

        if (isValid())
            initialize();
    }
}

TimelineCalendarWidget *QnWorkbenchNavigator::calendar() const
{
    return m_calendar;
}

void QnWorkbenchNavigator::setCalendar(TimelineCalendarWidget* calendar)
{
    if (m_calendar == calendar)
        return;

    if (m_calendar)
    {
        m_calendar->disconnect(this);

        if (isValid())
            deinitialize();
    }

    m_calendar = calendar;

    if (m_calendar)
    {
        connect(m_calendar, &QObject::destroyed, this, [this]() { setCalendar(nullptr); });

        if (isValid())
            initialize();
    }
}

bool QnWorkbenchNavigator::bookmarksModeEnabled() const
{
    return m_timeSlider && m_timeSlider->isBookmarksVisible();
}

void QnWorkbenchNavigator::setBookmarksModeEnabled(bool enabled)
{
    if (bookmarksModeEnabled() == enabled)
        return;

    NX_VERBOSE(this, "Setting bookmark mode %1", enabled ? "enabled" : "disabled");

    m_timeSlider->setBookmarksVisible(enabled);

    // Camera bookmark manager will be enabled automatically.

    emit bookmarksModeEnabledChanged();
}

bool QnWorkbenchNavigator::isValid()
{
    return m_timeSlider && m_timeScrollBar && m_calendar;
}

void QnWorkbenchNavigator::initialize()
{
    NX_ASSERT(isValid(), "we should definitely be valid here");
    if (!isValid())
        return;

    connect(workbench(), &Workbench::currentLayoutChanged,
        this, &QnWorkbenchNavigator::updateSliderOptions);

    connect(system()->cameraHistoryPool(), &QnCameraHistoryPool::cameraFootageChanged, this,
        [this](const QnSecurityCamResourcePtr & /* camera */)
        {
            updateFootageState();
        });

    connect(display(), &QnWorkbenchDisplay::widgetChanged, this,
        &QnWorkbenchNavigator::at_display_widgetChanged);
    connect(display(), &QnWorkbenchDisplay::widgetAdded, this,
        &QnWorkbenchNavigator::at_display_widgetAdded);
    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this,
        &QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved);
    connect(display()->beforePaintInstrument(), QnSignalingInstrumentActivated, this,
        [this](){ updateSliderFromReader(); });

    connect(m_timeSlider, &QnTimeSlider::timeChanged,
        this, &QnWorkbenchNavigator::at_timeSlider_valueChanged);
    connect(m_timeSlider, &QnTimeSlider::visibleChanged,
        this, &QnWorkbenchNavigator::updateLive);
    connect(m_timeSlider, &QnTimeSlider::sliderPressed,
        this, &QnWorkbenchNavigator::at_timeSlider_sliderPressed);
    connect(m_timeSlider, &QnTimeSlider::sliderReleased,
        this, &QnWorkbenchNavigator::at_timeSlider_sliderReleased);
    connect(m_timeSlider, &QnTimeSlider::timeChanged,
        this, &QnWorkbenchNavigator::updateScrollBarFromSlider);
    connect(m_timeSlider, &QnTimeSlider::timeRangeChanged,
        this, &QnWorkbenchNavigator::updateScrollBarFromSlider);
    connect(m_timeSlider, &QnTimeSlider::windowChanged,
        this, &QnWorkbenchNavigator::updateScrollBarFromSlider);
    connect(m_timeSlider, &QnTimeSlider::windowChanged,
        this, &QnWorkbenchNavigator::updateCalendarFromSlider);
    connect(m_timeSlider, &QnTimeSlider::customContextMenuRequested,
        this, &QnWorkbenchNavigator::at_timeSlider_customContextMenuRequested);
    connect(m_timeSlider, &QnTimeSlider::selectionPressed,
        this, &QnWorkbenchNavigator::at_timeSlider_selectionPressed);
    connect(m_timeSlider, &QnTimeSlider::thumbnailsVisibilityChanged,
        this, &QnWorkbenchNavigator::updateTimeSliderWindowSizePolicy);
    connect(m_timeSlider, &QnTimeSlider::thumbnailClicked,
        this, &QnWorkbenchNavigator::at_timeSlider_thumbnailClicked);

    connect(m_timeSlider, &QnTimeSlider::selectionChanged,
        this, &QnWorkbenchNavigator::timeSelectionChanged);

    m_timeSlider->setLiveSupported(isLiveSupported());

    m_timeSlider->setLineCount(SliderLineCount);
    m_timeSlider->setLineStretch(CurrentLine, 4.0);
    m_timeSlider->setLineStretch(SyncedLine, 1.0);
    m_timeSlider->setTimeRange(0ms, 24h); //< Wait for C++20 for days!
    m_timeSlider->setWindow(m_timeSlider->minimum(), m_timeSlider->maximum());

    connect(m_timeScrollBar, SIGNAL(valueChanged(qint64)), this, SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar, SIGNAL(pageStepChanged(qint64)), this, SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar, SIGNAL(sliderPressed()), this, SLOT(at_timeScrollBar_sliderPressed()));
    connect(m_timeScrollBar, SIGNAL(sliderReleased()), this, SLOT(at_timeScrollBar_sliderReleased()));
    m_timeScrollBar->installEventFilter(this);

    m_calendar->selection.connectNotifySignal(
        this, [this]() { updateTimeSliderWindowFromCalendar(); });

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = system()->serverTimeWatcher();
    connect(timeWatcher, &nx::vms::client::core::ServerTimeWatcher::displayOffsetsChanged,
        this, &QnWorkbenchNavigator::updateLocalOffset);

    connect(workbenchContext()->instance<QnWorkbenchUserInactivityWatcher>(),
        &QnWorkbenchUserInactivityWatcher::stateChanged,
        this,
        &QnWorkbenchNavigator::updateAutoPaused);

    connect(action(menu::ToggleShowreelModeAction), &QAction::toggled, this,
        &QnWorkbenchNavigator::updateAutoPaused);

    updateLines();
    updateCalendar();
    updateScrollBarFromSlider();
    updateTimeSliderWindowSizePolicy();
    updateAutoPaused();
}

void QnWorkbenchNavigator::deinitialize()
{
    NX_ASSERT(isValid(), "Instance should be still valid here.");
    if (!isValid())
        return;

    workbench()->disconnect(this);

    display()->disconnect(this);
    display()->beforePaintInstrument()->disconnect(this);

    m_timeSlider->setThumbnailLoadingManager({});
    m_timeSlider->disconnect(this);

    m_timeScrollBar->disconnect(this);
    m_timeScrollBar->removeEventFilter(this);

    m_calendar->disconnect(this);

    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = system()->serverTimeWatcher();
    timeWatcher->disconnect(this);

    m_currentWidget = nullptr;
    m_currentWidgetFlags = {};
}

menu::ActionScope QnWorkbenchNavigator::currentScope() const
{
    return menu::TimelineScope;
}

menu::Parameters QnWorkbenchNavigator::currentParameters(menu::ActionScope scope) const
{
    if (scope != menu::TimelineScope)
        return menu::Parameters();

    QnResourceWidgetList result;
    if (m_currentWidget)
        result.push_back(m_currentWidget);
    return menu::Parameters(result);
}

bool QnWorkbenchNavigator::isLiveSupported() const
{
    // TODO: #sivanov Just return cached value. Even better: replace m_lastLiveSupported with a
    // direct call to m_timeSlider (and store calclated value only once).
    const bool value = calculateIsLiveSupported();
    NX_ASSERT(value == m_lastLiveSupported);
    return value;
}

bool QnWorkbenchNavigator::isLive() const
{
    return isLiveSupported()
        && speed() > 0
        && m_timeSlider
        && (!m_timeSlider->isVisible() || m_timeSlider->isLive());
}

bool QnWorkbenchNavigator::setLive(bool live)
{
    if (live == isLive())
        return true;

    if (!isLiveSupported())
        return false;

    if (!m_timeSlider)
        return false;

    NX_VERBOSE(this, "Set live to %1", live);
    if (live)
    {
        m_timeSlider->setValue(m_timeSlider->maximum(), true);
    }
    else
    {
        m_timeSlider->setValue(m_timeSlider->maximum() - 1ms, false);
    }
    return true;
}

bool QnWorkbenchNavigator::isPlayingSupported() const
{
    if (!m_currentMediaWidget)
        return false;

    return m_currentMediaWidget->display()->archiveReader()
        && !m_currentMediaWidget->display()->isStillImage();
}

bool QnWorkbenchNavigator::hasVmaxInSync() const
{
    auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();

    const auto syncAllowed = streamSynchronizer->isEffective()
        && currentWidgetFlags().testFlag(QnWorkbenchNavigator::WidgetSupportsSync);

    auto isSync = syncAllowed && streamSynchronizer->isRunning();
    if (!isSync)
        return false;

    return std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(), resourceIsVmax);
}

bool QnWorkbenchNavigator::hasVmax() const
{
    return currentResourceIsVmax() || hasVmaxInSync();
}

bool QnWorkbenchNavigator::isTimelineRelevant() const
{
    return m_timelineRelevant;
}

bool QnWorkbenchNavigator::isPlaying() const
{
    if (!isPlayingSupported())
        return false;
    if (!m_currentMediaWidget)
        return false;

    return !m_currentMediaWidget->display()->archiveReader()->isMediaPaused();
}

bool QnWorkbenchNavigator::setPlaying(bool playing)
{
    if (playing == isPlaying())
        return true;

    if (!m_currentMediaWidget)
        return false;

    if (!isPlayingSupported())
        return false;

    if (playing && m_autoPaused)
        return false;

    m_pausedOverride = false;
    m_autoPaused = false;

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
    QnCamDisplay *camDisplay = m_currentMediaWidget->display()->camDisplay();
    if (playing)
    {
        if (reader->isRealTimeSource())
        {
            /* Media was paused while on live. Jump to archive when resumed. */
            qint64 time = m_currentMediaWidget->display()->camDisplay()->getExternalTime();
            reader->resumeMedia();
            if (time != (qint64)AV_NOPTS_VALUE && !reader->isReverseMode())
                reader->directJumpToNonKeyFrame(time + 1);
        }
        else
        {
            reader->resumeMedia();
        }
        camDisplay->playAudio(true);

        if (qFuzzyIsNull(speed()))
        {
            const auto speed = cachedSpeed(m_currentMediaWidget);
            setSpeed(qFuzzyIsNull(speed) ? 1.0 : speed);
        }
    }
    else
    {
        cacheSpeed(speed(), m_currentMediaWidget);
        reader->pauseMedia();
        setSpeed(0.0);
    }

    updatePlaying();
    return true;
}

qreal QnWorkbenchNavigator::speed() const
{
    if (!isPlayingSupported())
        return 0.0;

    if (QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader())
        return reader->getSpeed();

    return 1.0;
}

void QnWorkbenchNavigator::setSpeed(qreal speed)
{
    speed = speedRange().boundSpeed(speed);
    if (qFuzzyEquals(speed, this->speed()))
        return;

    if (!m_currentMediaWidget)
        return;

    if (QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader())
    {
        reader->setSpeed(speed);

        setPlaying(!qFuzzyIsNull(speed));

        if (speed <= 0.0)
            setLive(false);
        else if (!hasArchive())
            setLive(true);

        updateSpeed();
    }
}

bool QnWorkbenchNavigator::hasArchive() const
{
    return m_hasArchive;
}

bool QnWorkbenchNavigator::isRecording() const
{
    return m_isRecording;
}

bool QnWorkbenchNavigator::syncIsForced() const
{
    return m_syncIsForced;
}

void QnWorkbenchNavigator::updateFootageState()
{
    updateHasArchive();
    updateIsRecording();
}

void QnWorkbenchNavigator::updateHasArchive()
{
    const bool newValue = std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(),
        [this](const QnMediaResourcePtr& resource)
        {
            return hasArchiveForCamera(resource.dynamicCast<QnSecurityCamResource>());
        });

    if (m_hasArchive == newValue)
        return;

    m_hasArchive = newValue;
    emit hasArchiveChanged();
}

void QnWorkbenchNavigator::updateIsRecording(bool forceOn)
{
    bool newValue = forceOn ||
        std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(),
            [](const QnMediaResourcePtr& resource)
            {
                auto camera = resource.dynamicCast<QnSecurityCamResource>();
                if (!camera)
                    return false;

                if (!ResourceAccessManager::hasPermissions(camera, Qn::ViewFootagePermission))
                    return false;

                return camera->getStatus() == nx::vms::api::ResourceStatus::recording;
            });

    if (m_isRecording == newValue)
        return;

    m_recordingStartUtcMs = newValue ? qnSyncTime->currentMSecsSinceEpoch() : 0;

    m_isRecording = newValue;
    emit isRecordingChanged();
}

bool QnWorkbenchNavigator::currentWidgetHasVideo() const
{
    return m_currentMediaWidget && m_currentMediaWidget->hasVideo();
}

QnSpeedRange QnWorkbenchNavigator::speedRange() const
{
    return m_currentMediaWidget
        ? m_currentMediaWidget->speedRange()
        : QnSpeedRange();
}

qreal QnWorkbenchNavigator::minimalSpeed() const
{
    return -speedRange().backward;
}

qreal QnWorkbenchNavigator::maximalSpeed() const
{
    return speedRange().forward;
}

qint64 QnWorkbenchNavigator::positionUsec() const
{
    if (!m_currentMediaWidget)
        return 0;

    return m_currentMediaWidget->display()->camDisplay()->getExternalTime();
}

void QnWorkbenchNavigator::setPosition(qint64 positionUsec)
{
    if (!m_currentMediaWidget)
        return;

    const auto positionMs = positionUsec < 0 || positionUsec == DATETIME_NOW
        ? positionUsec //< Special time value.
        : positionUsec / 1000;

    m_currentMediaWidget->setPosition(positionMs);
    emit positionChanged();
}

QnTimePeriod QnWorkbenchNavigator::timelineRange() const
{
    return m_timeSlider && m_timelineRelevant
        ? QnTimePeriod::fromInterval(m_timeSlider->minimum(), m_timeSlider->maximum())
        : QnTimePeriod();
}

void QnWorkbenchNavigator::addSyncedWidget(QnMediaResourceWidget *widget)
{
    if (!NX_ASSERT(widget))
        return;

    auto syncedResource = widget->resource();
    auto systemContext = SystemContext::fromResource(syncedResource->toResourcePtr());
    if (!NX_ASSERT(systemContext))
        return;

    m_syncedWidgets.insert(widget);
    ++m_syncedResources[syncedResource];

    connect(syncedResource->toResourcePtr().get(), &QnResource::parentIdChanged,
        this, &QnWorkbenchNavigator::updateLocalOffset);
    connect(syncedResource->toResourcePtr().get(), &QnResource::propertyChanged,
        this, &QnWorkbenchNavigator::updateLocalOffset);

    if (auto loader = systemContext->cameraDataManager()->loader(syncedResource))
    {
        core::CachingCameraDataLoader::AllowedContent content =
            {Qn::RecordingContent, Qn::MotionContent};

        if (selectedExtraContent() == Qn::AnalyticsContent
            && m_currentMediaWidget
            && m_currentMediaWidget->resource() == syncedResource)
        {
            content.insert(Qn::AnalyticsContent);
            // If the widget with the same resource is already present, we must not modify
            // it's loader at all.
            NX_ASSERT(loader->allowedContent() == content);
        }

        loader->setAllowedContent(content);
    }

    updateCurrentWidget();

    if (workbench() && !workbench()->isInLayoutChangeProcess())
        updateSyncedPeriods();

    if (!widget->isZoomWindow())
        updateHistoryForCamera(widget->resource()->toResourcePtr().dynamicCast<QnSecurityCamResource>());

    updateLines();
    updateSyncIsForced();
    updateLiveSupported();
}

void QnWorkbenchNavigator::removeSyncedWidget(QnMediaResourceWidget *widget)
{
    if (!m_syncedWidgets.remove(widget))
        return;

    auto syncedResource = widget->resource();

    disconnect(syncedResource->toResourcePtr().get(), &QnResource::parentIdChanged,
        this, &QnWorkbenchNavigator::updateLocalOffset);

    if (display() && !display()->isChangingLayout())
    {
        if (m_syncedWidgets.contains(m_currentMediaWidget))
            updateItemDataFromSlider(widget);
    }

    bool noMoreWidgetsOfThisResource = true;

    auto iter = m_syncedResources.find(syncedResource);
    if (iter != m_syncedResources.end())
    {
        NX_ASSERT(iter.value() > 0);
        noMoreWidgetsOfThisResource = (--iter.value() <= 0);
        if (noMoreWidgetsOfThisResource)
            m_syncedResources.erase(iter);
    }

    m_motionIgnoreWidgets.remove(widget);

    if (noMoreWidgetsOfThisResource)
    {
        auto systemContext = SystemContext::fromResource(syncedResource->toResourcePtr());
        if (!NX_ASSERT(systemContext))
            return;

        if (auto loader = systemContext->cameraDataManager()->loader(
            syncedResource,
            /*createIfNotExists*/ false))
        {
            loader->setMotionSelection({});
            loader->setAllowedContent({});
        }
    }

    updateCurrentWidget();
    if (workbench() && !workbench()->isInLayoutChangeProcess())
        updateSyncedPeriods(); /* Full rebuild on widget removing. */
    updateLines();
    updateSyncIsForced();
    updateLiveSupported();
}

QnResourceWidget* QnWorkbenchNavigator::currentWidget() const
{
    return m_currentWidget;
}

QnMediaResourceWidget* QnWorkbenchNavigator::currentMediaWidget() const
{
    return m_currentMediaWidget;
}

QnResourcePtr QnWorkbenchNavigator::currentResource() const
{
    return m_currentWidget ? m_currentWidget->resource() : QnResourcePtr();
}

bool QnWorkbenchNavigator::currentResourceIsVmax() const
{
    return resourceIsVmax(currentResource().dynamicCast<QnMediaResource>());
}

QnWorkbenchNavigator::WidgetFlags QnWorkbenchNavigator::currentWidgetFlags() const
{
    return m_currentWidgetFlags;
}

void QnWorkbenchNavigator::updateItemDataFromSlider(QnResourceWidget *widget) const
{
    if (!widget || !m_timeSlider)
        return;

    QnWorkbenchItem* item = widget->item();
    if (!NX_ASSERT(item))
        return;

    auto layout = workbench()->currentLayoutResource();

    QnTimePeriod window(m_timeSlider->windowStart(),
        m_timeSlider->windowEnd() - m_timeSlider->windowStart());

    // TODO: #sivanov Check that widget supports live.
    if (m_timeSlider->windowEnd() == m_timeSlider->maximum())
        window.durationMs = QnTimePeriod::kInfiniteDuration;

    layout->setItemData(
        item->uuid(),
        Qn::ItemSliderWindowRole,
        QVariant::fromValue<QnTimePeriod>(window));

    if (layout->isPreviewSearchLayout())
        return;

    QnTimePeriod selection;
    if (m_timeSlider->isSelectionValid())
    {
        selection = QnTimePeriod(m_timeSlider->selectionStart(),
            m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());
    }
    layout->setItemData(
        item->uuid(),
        Qn::ItemSliderSelectionRole,
        QVariant::fromValue<QnTimePeriod>(selection));
}

void QnWorkbenchNavigator::updateSliderFromItemData(QnResourceWidget *widget)
{
    if (!widget || !m_timeSlider)
        return;

    QnWorkbenchItem *item = widget->item();

    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();

    const bool preferToPreserveWindow = !m_sliderWindowInvalid && window.isNull();
    if (preferToPreserveWindow
        && m_timeSlider->value() >= m_timeSlider->windowStart()
        && m_timeSlider->value() <= m_timeSlider->windowEnd())
    {
        /* Just skip window initialization. */
    }
    else
    {
        if (window.isEmpty())
            window = QnTimePeriod::anytime();

        milliseconds windowStart = milliseconds(window.startTimeMs);
        milliseconds windowEnd = window.isInfinite()
            ? m_timeSlider->maximum()
            : milliseconds(window.startTimeMs + window.durationMs);
        if (windowStart < m_timeSlider->minimum())
        {
            milliseconds delta = m_timeSlider->minimum() - windowStart;
            windowStart += delta;
            windowEnd += delta;
        }
        m_timeSlider->setWindow(windowStart, windowEnd);
    }

    QnTimePeriod selection = item->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();
    m_timeSlider->setSelectionValid(!selection.isNull());
    m_timeSlider->setSelection(milliseconds(selection.startTimeMs),
        milliseconds(selection.startTimeMs + selection.durationMs));
}

void QnWorkbenchNavigator::jumpBackward()
{
    if (!m_currentMediaWidget)
        return;

    const auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    qint64 pos = reader->startTime();

    if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        const auto content = selectedExtraContent();
        QnTimePeriodList periods = loader->periods(content);
        if (content == Qn::RecordingContent)
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        if (!periods.empty())
        {
            if (m_timeSlider->isLive())
            {
                pos = periods.last().startTimeMs * 1000;
            }
            else
            {
                /* We want to jump relatively to current reader position. */
                const auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(reader->currentTime());

                QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTimeMs.count(), true);
                if (itr != periods.cbegin())
                    --itr;

                pos = itr->startTimeMs * 1000;
                if (reader->isReverseMode() && !itr->isInfinite())
                    pos += itr->durationMs * 1000;
            }
        }
    }

    reader->jumpTo(pos, pos);
    updateSliderFromReader();
    emit positionChanged();
}

void QnWorkbenchNavigator::jumpForward()
{
    if (!m_currentMediaWidget)
        return;

    const auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    /* Default value should never be used, adding just in case of black magic. */
    qint64 pos = DATETIME_NOW;
    if (!(m_currentWidgetFlags & WidgetSupportsPeriods))
    {
        pos = reader->endTime();
    }
    else if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        const auto content = selectedExtraContent();
        QnTimePeriodList periods = loader->periods(content);
        if (content == Qn::RecordingContent)
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        /* We want to jump relatively to current reader position. */
        const auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(reader->currentTime());

        QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTimeMs.count(), true);
        if (itr != periods.cend() && currentTimeMs.count() >= itr->startTimeMs)
            ++itr;

        if (itr == periods.cend() || m_timeSlider->isLive())
        {
            /* Do not make step forward to live if we are playing backward. */
            if (reader->isReverseMode())
                return;
            pos = DATETIME_NOW;
        }
        else if (reader->isReverseMode() && itr->isInfinite())
        {
            /* Do not make step forward to live if we are playing backward. */
            --itr;
            pos = itr->startTimeMs * 1000;
        }
        else
        {
            pos = (itr->startTimeMs + (reader->isReverseMode() ? itr->durationMs : 0)) * 1000;
        }
    }

    reader->jumpTo(pos, pos);
    updateSliderFromReader();
    emit positionChanged();
}

bool QnWorkbenchNavigator::canJump() const
{
    if (const auto loader = loaderByWidget(m_currentMediaWidget))
        return !loader->periods(Qn::RecordingContent).empty();

    return false;
}

void QnWorkbenchNavigator::fastForward()
{
    if (!m_currentMediaWidget)
        return;

    const auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    /* Default value should never be used, adding just in case of black magic. */
    qint64 pos = DATETIME_NOW;
    if (!(m_currentWidgetFlags & WidgetSupportsPeriods))
    {
        pos = reader->endTime();
    }
    else if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent);
        periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        const auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            m_timeSlider->sliderTimePosition());
        auto curPeriod = periods.findNearestPeriod(currentTimeMs.count(), false);
        if (curPeriod == periods.end())
            return; //< Reader currently in some invalid state.

        std::chrono::milliseconds posMs;
        if (curPeriod->contains(currentTimeMs + kShiftStepMs))
            posMs = currentTimeMs + kShiftStepMs;
        else if (auto next = curPeriod + 1; next != periods.end())
            posMs = (curPeriod+1)->startTime();
        else
            posMs = m_timeSlider->maximum();

        pos = posMs.count() * 1000;
    }

    if (pos >= m_timeSlider->maximum().count() * 1000)
    {
        if (!setLive(true) && m_timeSlider)
            m_timeSlider->setValue(m_timeSlider->maximum() - 1ms, false);
        return;
    }

    reader->directJumpToNonKeyFrame(pos);
    updateSliderFromReader();
    emit positionChanged();
}

void QnWorkbenchNavigator::rewindOnDoubleClick()
{
    if (!m_currentMediaWidget)
        return;

    const auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    // Default value should never be used, adding just in case of black magic.
    qint64 pos = DATETIME_NOW;
    if (!(m_currentWidgetFlags & WidgetSupportsPeriods))
    {
        pos = reader->endTime();
    }
    else if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent);
        periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        // We want to jump relatively to current reader position.
        const auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            m_timeSlider->sliderTimePosition());
        auto curPeriod = periods.findNearestPeriod(currentTimeMs.count(), false);
        if (curPeriod == periods.end())
            return; //< Reader currently in some invalid state.

        auto delta = currentTimeMs - curPeriod->startTime();
        if (delta >= 1s || curPeriod == periods.begin())
        {
            pos = curPeriod->startTimeMs * 1000; //< Transform to microseconds
        }
        else
        {
            const auto prevPeriod = std::prev(curPeriod);
            auto posMs = std::max(prevPeriod->startTime(), prevPeriod->endTime() - kShiftStepMs);
            pos = posMs.count() * 1000;
        }
    }

    reader->directJumpToNonKeyFrame(pos);
    updateSliderFromReader();
    emit positionChanged();
}

void QnWorkbenchNavigator::rewind(bool canJumpToPrevious)
{
    if (!m_currentMediaWidget)
        return;

    if (m_skip1Step)
    {
        m_skip1Step = false;
        return;
    }

    const auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;
    m_pausedOverride = false;

    /* Default value should never be used, adding just in case of black magic. */
    qint64 pos = DATETIME_NOW;
    if (!(m_currentWidgetFlags & WidgetSupportsPeriods))
    {
        pos = reader->endTime();
    }
    else if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent);
        periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        /* We want to jump relatively to current reader position. */
        const auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            m_timeSlider->sliderTimePosition());
        auto curPeriod = periods.findNearestPeriod(currentTimeMs.count(), false);
        if (curPeriod == periods.end())
            return; //< Reader currently in some invalid state.

        std::chrono::milliseconds posMs = currentTimeMs - kShiftStepMs;
        if (!curPeriod->contains(posMs))
        {
            if (curPeriod == periods.begin() || !canJumpToPrevious)
            {
                posMs = curPeriod->startTime();
            }
            else
            {
                const auto prevPeriod = std::prev(curPeriod);
                posMs = std::max(prevPeriod->startTime(), prevPeriod->endTime() - kShiftStepMs);
                m_skip1Step = true;
            }
        }

        pos = posMs.count() * 1000;
    }

    reader->directJumpToNonKeyFrame(pos);
    updateSliderFromReader();
    emit positionChanged();
}

void QnWorkbenchNavigator::stepBackward()
{
    if (!m_currentMediaWidget)
        return;

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    /* Here we want to know real reader time. */
    qint64 currentTime = m_currentMediaWidget->display()->camDisplay()->getExternalTime();

    if (!reader->isSkippingFrames()
        && currentTime > reader->startTime()
        && !m_currentMediaWidget->display()->camDisplay()->isBuffering())
    {

        reader->previousFrame(currentTime);
        updateSliderFromReader();
    }

    emit positionChanged();
}

void QnWorkbenchNavigator::stepForward()
{
    if (!m_currentMediaWidget)
        return;

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    reader->nextFrame();
    updateSliderFromReader();

    emit positionChanged();
}

void QnWorkbenchNavigator::setPlayingTemporary(bool playing)
{
    if (!m_currentMediaWidget)
        return;

    if (playing)
        m_currentMediaWidget->display()->archiveReader()->resumeMedia();
    else
        m_currentMediaWidget->display()->archiveReader()->pauseMedia();
}

// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void QnWorkbenchNavigator::updateCentralWidget()
{
    // TODO: #sivanov get rid of the central widget - it is used ONLY as a previous/current value
    // in the layout change process
    QnResourceWidget *centralWidget = display()->widget(Qn::CentralRole);
    if (m_centralWidget == centralWidget)
        return;

    m_centralWidgetConnections = {};

    m_centralWidget = centralWidget;

    if (m_centralWidget)
    {
        m_centralWidgetConnections << connect(m_centralWidget->resource().get(),
            &QnResource::parentIdChanged,
            this,
            &QnWorkbenchNavigator::updateLocalOffset);
    }

    updateCurrentWidget();
}

void QnWorkbenchNavigator::updateCurrentWidget()
{
    QnResourceWidget* widget = m_centralWidget;
    if (m_currentWidget == widget)
        return;

    if (selectedExtraContent() == Qn::AnalyticsContent)
    {
        if (auto loader = loaderByWidget(m_currentMediaWidget, /*createIfNotExists*/ false))
        {
            auto allowedContent = loader->allowedContent();
            allowedContent.erase(Qn::AnalyticsContent);
            loader->setAllowedContent(allowedContent);
        }
    }

    QnMediaResourceWidget* mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);

    const auto previousResource = currentResource();

    emit currentWidgetAboutToBeChanged();

    WidgetFlags previousWidgetFlags = m_currentWidgetFlags;

    m_currentWidgetConnections = {};

    if (m_currentWidget)
    {
        if (m_timeSlider)
            m_timeSlider->setThumbnailLoadingManager(nullptr);

        const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
        if (streamSynchronizer->isRunning() && (m_currentWidgetFlags & WidgetSupportsPeriods))
        {
            // TODO: #sivanov Should it be done at every selection change?
            for (auto widget: m_syncedWidgets)
                updateItemDataFromSlider(widget);
        }
        else
        {
            updateItemDataFromSlider(m_currentWidget);
        }
    }
    else
    {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid = true;
    }

    if (display() && display()->isChangingLayout())
    {
        // clear current widget state to avoid incorrect behavior when closing the layout
        // see: Bug #1341: Selection on timeline aren't displayed after thumbnails searching
        // see: Bug #1344: If make a THMB search from a layout with a result of THMB search, Timeline are not marked properly
        m_currentWidget = nullptr;
        m_currentMediaWidget = nullptr;
    }
    else
    {
        m_currentWidget = widget;
        m_currentMediaWidget = mediaWidget;
    }

    if (m_currentWidget)
    {
        m_currentWidgetConnections << connect(m_currentWidget->resource().get(),
            &QnResource::nameChanged,
            this,
            &QnWorkbenchNavigator::updateLines);

        if (m_currentMediaWidget)
        {
            m_currentWidgetConnections
                << connect(m_currentMediaWidget, &QnMediaResourceWidget::speedChanged,
                    this, &QnWorkbenchNavigator::updateSpeed);
        }
    }

    if (m_currentMediaWidget && selectedExtraContent() == Qn::AnalyticsContent)
    {
        if (auto loader = loaderByWidget(m_currentMediaWidget, /*createIfNotExists*/ true))
        {
            auto allowedContent = loader->allowedContent();
            allowedContent.insert(Qn::AnalyticsContent);
            loader->setAllowedContent(allowedContent);
        }
    }

    m_pausedOverride = false;
    m_currentWidgetLoaded = false;

    initializePositionAnimations();

    updateCurrentWidgetFlags();
    updatePlayingSupported();
    updateLines();
    updateCalendar();

    if (!(isCurrentWidgetSynced() && previousWidgetFlags.testFlag(WidgetSupportsSync)) && m_currentWidget)
    {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid |= (m_currentWidgetFlags.testFlag(WidgetUsesUTC)
            != previousWidgetFlags.testFlag(WidgetUsesUTC));
    }

    // Resets timeline settings before applying the new ones. Should be done before update from
    // the reader.
    updateTimelineRelevancy();

    updateSliderFromReader(UpdateSliderMode::ForcedUpdate);
    if (m_timeSlider)
        m_timeSlider->finishAnimations();

    if (m_currentMediaWidget)
    {
        const auto callback =
            [this]()
            {
                // TODO: #rvasilenko why should we make these delayed calls at all?
                updatePlaying();
                updateSpeed();
                if (speed() <= 0 && hasVmax())
                    setPlaying(false);
            };

        executeDelayedParented(callback, this);
    }

    updateLocalOffset();
    updateCurrentPeriods();
    updateLive();
    updatePlaying();
    updateSpeedRange();
    updateSpeed();
    updateThumbnailsLoader();
    updatePlaybackMask();

    emit currentWidgetChanged();

    if (previousResource != currentResource())
        emit currentResourceChanged();
}

void QnWorkbenchNavigator::updateLocalOffset()
{
    qint64 localOffset = m_currentMediaWidget
        ? m_currentMediaWidget->systemContext()->serverTimeWatcher()->displayOffset(
            m_currentMediaWidget->resource())
        : 0;

    if (m_timeSlider)
        m_timeSlider->setLocalOffset(milliseconds(localOffset));
    if (m_calendar)
        m_calendar->displayOffset = localOffset;
}

QnWorkbenchNavigator::WidgetFlags QnWorkbenchNavigator::calculateResourceWidgetFlags(const QnResourcePtr& resource) const
{
    WidgetFlags result;

    if (resource.dynamicCast<QnSecurityCamResource>())
        result |= WidgetSupportsLive | WidgetSupportsPeriods;

    if (resource->hasFlags(Qn::periods))
        result |= WidgetSupportsPeriods;

    if (resource->hasFlags(Qn::utc))
        result |= WidgetUsesUTC;

    if (resource->hasFlags(Qn::sync))
        result |= WidgetSupportsSync;

    if (resource->hasFlags(Qn::virtual_camera))
        result &= ~WidgetSupportsLive;

    return result;
}

void QnWorkbenchNavigator::updateCurrentWidgetFlags()
{
    WidgetFlags flags;

    if (m_currentWidget)
    {
        flags = calculateResourceWidgetFlags(m_currentWidget->resource());

        if (workbench()->currentLayout()->isPreviewSearchLayout())
            flags &= ~(WidgetSupportsLive | WidgetSupportsSync);

        QnTimePeriod period = workbench()->currentLayout()->resource()
            ? workbench()->currentLayout()->resource()->localRange()
            : QnTimePeriod();
        if (!period.isNull())
            flags &= ~WidgetSupportsLive;
    }

    if (m_currentWidgetFlags == flags)
        return;

    m_currentWidgetFlags = flags;

    updateSliderOptions();
    updateLiveSupported();
}

void QnWorkbenchNavigator::updateSliderOptions()
{
    if (!m_timeSlider)
        return;

    m_timeSlider->setOption(QnTimeSlider::UseUTC, m_currentWidgetFlags & WidgetUsesUTC);

    m_timeSlider->setOption(QnTimeSlider::ClearSelectionOnClick,
        !workbench()->currentLayout()->isPreviewSearchLayout());

    bool selectionEditable = (bool) workbench()->currentLayout()->resource();
    m_timeSlider->setOption(QnTimeSlider::SelectionEditable, selectionEditable);
    if (!selectionEditable)
        m_timeSlider->setSelectionValid(false);
}

VariantAnimator* QnWorkbenchNavigator::createPositionAnimator()
{
    auto positionGetter = [this](const QObject* target) -> qint64
    {
        Q_UNUSED(target);
        return m_animatedPosition;
    };

    auto positionSetter = [this](const QObject* target, const QVariant& value)
    {
        Q_UNUSED(target);
        m_animatedPosition = value.value<qint64>();
    };

    VariantAnimator* animator = new VariantAnimator(this);
    animator->setTimer(InstrumentManager::animationTimer(mainWindow()->scene()));
    animator->setAccessor(newAccessor(positionGetter, positionSetter));
    animator->setTargetObject(this);
    return animator;
}

void QnWorkbenchNavigator::initializePositionAnimations()
{
    if (m_positionAnimator)
        m_positionAnimator->stop();
    else
        m_positionAnimator = createPositionAnimator();
}

void QnWorkbenchNavigator::stopPositionAnimations()
{
    if (m_positionAnimator)
        m_positionAnimator->stop();
}

/* Advance timeline forward or backward with current playback speed (when media updates are less frequent than display updates) */
void QnWorkbenchNavigator::timelineAdvance(qint64 fromMs)
{
    qreal speedFactor = speed();
    if (qFuzzyIsNull(speedFactor))
    {
        m_positionAnimator->stop();
        return;
    }

    m_positionAnimator->setSpeed(qAbs(speedFactor) * 1000.0);

    m_positionAnimator->animateTo(fromMs +
        static_cast<qint64>(kAdvanceIntervalMs * qSign(speedFactor)));
}

/* Advance timeline forward or backward with speed adjusted to position deviation (this is the main animation mode) */
void QnWorkbenchNavigator::timelineCorrect(qint64 toMs)
{
    qreal speedFactor = speed();
    if (qFuzzyIsNull(speedFactor))
    {
        m_animatedPosition = toMs;
        m_positionAnimator->stop();
        return;
    }

    const qint64 kCorrectionIntervalMs = kCatchUpThresholdMs * 2;
    qreal correctionTimeMs = kCorrectionIntervalMs * speedFactor;

    qint64 delta = toMs - m_animatedPosition;
    qreal correctionFactor = (correctionTimeMs + delta) / correctionTimeMs;

    m_positionAnimator->setSpeed(qAbs(speedFactor * correctionFactor) * 1000.0);

    m_positionAnimator->animateTo(toMs +
        static_cast<qint64>(kAdvanceIntervalMs * qSign(speedFactor)));
}

/* Quickly catch timeline up with current media position */
void QnWorkbenchNavigator::timelineCatchUp(qint64 toMs)
{
    const qint64 kMinCatchUpMsPerSecond = kCatchUpThresholdMs;
    qreal catchUpMsPerSecond = qMax(qAbs(toMs - m_animatedPosition), kMinCatchUpMsPerSecond) * kCatchUpTimeFactor;

    m_positionAnimator->setSpeed(catchUpMsPerSecond);
    m_positionAnimator->animateTo(toMs);

    emit positionChanged(); //< We use it for Video Wall sync.
}

bool QnWorkbenchNavigator::isTimelineCatchingUp() const
{
    return m_positionAnimator
        && m_positionAnimator->isRunning()
        && m_positionAnimator->targetValue() == m_previousMediaPosition;
}

bool QnWorkbenchNavigator::isCurrentWidgetSynced() const
{
    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    return streamSynchronizer->isRunning() && m_currentWidgetFlags.testFlag(WidgetSupportsSync);
}

void QnWorkbenchNavigator::connectToContext(SystemContext* systemContext)
{
    // Cloud layouts context does not have camera data.
    if (auto cameraDataManager = systemContext->cameraDataManager())
    {
        connect(cameraDataManager,
            &QnCameraDataManager::periodsChanged,
            this,
            &QnWorkbenchNavigator::updatePeriods);
    }
}

bool QnWorkbenchNavigator::syncEnabled() const
{
    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    return streamSynchronizer->state().isSyncOn;
}

void QnWorkbenchNavigator::updateSliderFromReader(UpdateSliderMode mode)
{
    if (!m_timeSlider)
        return;

    if (!m_currentMediaWidget)
        return;

    if (m_timeSlider->isSliderDown())
        return;

    auto reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    const bool keepInWindow = mode == UpdateSliderMode::KeepInWindow;
    QScopedValueRollback<bool> guard(m_updatingSliderFromReader, true);

    auto searchState = workbench()->currentLayout()->data(
        Qn::LayoutSearchStateRole).value<ThumbnailsSearchState>();
    bool isSearch = searchState.step > 0;

    qint64 startTimeMSec = 0;
    qint64 endTimeMSec = 0;

    bool widgetLoaded = false;
    bool isLiveSupportedInReader = false;
    bool timelineWindowIsNearLive = false;
    if (isSearch)
    {
        endTimeMSec = searchState.period.endTimeMs();
        startTimeMSec = searchState.period.startTimeMs;
        widgetLoaded = true;
    }
    else
    {
        const qint64 startTimeUSec = reader->startTime();
        const qint64 endTimeUSec = reader->endTime();
        widgetLoaded = startTimeUSec != AV_NOPTS_VALUE && endTimeUSec != AV_NOPTS_VALUE;
        isLiveSupportedInReader = endTimeUSec == DATETIME_NOW;

        /* This can also be true if the current widget has no archive at all and all other synced widgets still have not received rtsp response. */
        bool noRecordedPeriodsFound = startTimeUSec == DATETIME_NOW;

        if (!widgetLoaded)
        {
            startTimeMSec = m_timeSlider->minimum().count();
            endTimeMSec = m_timeSlider->maximum().count();
        }
        else if (noRecordedPeriodsFound)
        {
            const auto item = m_currentMediaWidget->item();
            const bool hasSpecifiedSliderWindow =
                item->data<QnTimePeriod>(Qn::ItemSliderWindowRole).isValid();

            if (!qnRuntime->isAcsMode())
            {
                // Recording has been just started, no archive received yet.
                if (isRecording() && !hasSpecifiedSliderWindow)
                {
                    // Set to last 10 seconds.
                    endTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
                    startTimeMSec = endTimeMSec - kTimelineWindowNearLive;
                    timelineWindowIsNearLive = true;
                }
                else
                {
                    // Trying to display data from the loaded chunks.
                    const auto periods =
                        m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent);
                    // We shouldn't do anything here until we receive actual data.
                    if (periods.empty())
                        return;

                    // Set to periods limits.
                    startTimeMSec = periods.first().startTimeMs;
                    endTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
                }
            }
        }
        else
        {
            /* Both values are valid. */
            startTimeMSec = startTimeUSec / 1000;
            endTimeMSec = endTimeUSec == DATETIME_NOW
                ? qnSyncTime->currentMSecsSinceEpoch()
                : endTimeUSec / 1000;

            if(m_currentWidget->resource()->hasFlags(Qn::virtual_camera))
            {
                bool onlyVirtualCameras = std::all_of(m_syncedWidgets.begin(), m_syncedWidgets.end(),
                    [](QnMediaResourceWidget* widget)
                    {
                        return widget->resource()->toResourcePtr()->hasFlags(Qn::virtual_camera);
                    });

                if (onlyVirtualCameras)
                {
                    QnTimePeriodList periods = m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent);
                    if (!periods.empty() && !periods.back().isInfinite())
                        endTimeMSec = periods.back().endTimeMs();
                }
            }
        }
    }

    if (m_sliderWindowInvalid || timelineWindowIsNearLive != m_timelineWindowIsNearLive)
    {
        m_timeSlider->finishAnimations();
        m_timeSlider->invalidateWindow();
    }
    m_timelineWindowIsNearLive = timelineWindowIsNearLive;

    NX_TRACE(this, "Updating slider from reader. IsLiveSuppored: %1", isLiveSupportedInReader);
    m_timeSlider->setTimeRange(milliseconds(startTimeMSec), milliseconds(endTimeMSec));

    if (m_calendar)
    {
        m_calendar->range = {
            QDateTime::fromMSecsSinceEpoch(startTimeMSec + m_calendar->displayOffset),
            QDateTime::fromMSecsSinceEpoch(endTimeMSec + m_calendar->displayOffset)};
    }

    if (!m_currentWidgetLoaded && widgetLoaded && !isSearch
        && display()->widgets().size() == 1
        && m_currentWidget->resource()->hasFlags(Qn::virtual_camera)
        && isLivePosition(m_currentWidget))
    {
        setPosition(startTimeMSec * 1000);
    }

    if (!m_pausedOverride)
    {
        // TODO: #sivanov #vkutin Refactor logic.
        auto usecTimeForWidget = [isSearch, this](QnMediaResourceWidget *mediaWidget) -> qint64
        {
            if (mediaWidget->display()->camDisplay()->isRealTimeSource())
                return DATETIME_NOW;

            const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
            qint64 timeUSec;
            if (isCurrentWidgetSynced())
                timeUSec = streamSynchronizer->state().timeUs; // Fetch "current" time instead of "displayed"
            else
                timeUSec = mediaWidget->display()->camDisplay()->getExternalTime();

            if (timeUSec == AV_NOPTS_VALUE)
                timeUSec = -1;

            if (isSearch && timeUSec < 0)
            {
                timeUSec = mediaWidget->item()->data<qint64>(Qn::ItemTimeRole, -1);
                if (timeUSec != DATETIME_NOW && timeUSec >= 0)
                    timeUSec *= 1000;
            }

            return timeUSec;
        };

        auto usecToMsec = [endTimeMSec, this](qint64 timeUSec) -> qint64
        {
            return timeUSec == DATETIME_NOW
                ? endTimeMSec
                : (timeUSec < 0 ? m_timeSlider->value().count() : timeUSec / 1000);
        };

        qint64 timeUSec = widgetLoaded
            ? usecTimeForWidget(m_currentMediaWidget)
            : isLiveSupported() ? DATETIME_NOW : startTimeMSec;

        qint64 timeMSec = usecToMsec(timeUSec);

        // If in Live we change speed from 1 downwards, we go out of Live, but then receive next
        // frame from camera which immediately pushes us back to Live. Preventing it:
        if (timeMSec >= m_timeSlider->maximum().count() && speed() <= 0.0)
            timeMSec = m_timeSlider->maximum().count() - 1;

        const bool isLive = (timeUSec == DATETIME_NOW);
        NX_TRACE(this, "Updating slider from reader. Is playing live: %1", isLive);
        NX_ASSERT(isLiveSupported() == m_timeSlider->isLiveSupported(),
            "Navigator supports live: %1, time slider: %2",
            isLiveSupported(),
            m_timeSlider->isLiveSupported());

        // If we are displaying live and timeslider is already in live, skip all animations.
        if (isLive && m_timeSlider->isLive())
        {
            NX_ASSERT(isLiveSupported());
            stopPositionAnimations();
            m_animatedPosition = timeMSec;
        }
        else if (!keepInWindow || m_sliderDataInvalid)
        {
            /* Position was reset: */
            m_animatedPosition = timeMSec;
            m_previousMediaPosition = timeMSec;
            timelineAdvance(timeMSec);
        }
        else
        {
            /* If media position was updated at this tick or timeline is catching up: */
            if (m_previousMediaPosition != timeMSec || isTimelineCatchingUp())
            {
                qint64 delta = timeMSec - m_animatedPosition;

                // See VMS-2657
                const bool canOmitAnimation = m_currentWidget
                    && m_currentWidget->resource()->flags().testFlag(Qn::local_media)
                    && !m_currentWidget->resource()->flags().testFlag(Qn::sync);

                if (qAbs(delta) < m_timeSlider->msecsPerPixel().count() ||
                    (canOmitAnimation && delta * speed() < 0))
                {
                    // If distance is less than 1 pixel or we catch up backwards on
                    // local media file do it instantly
                    m_animatedPosition = timeMSec;
                    timelineAdvance(timeMSec);
                }
                else
                {
                    /* If media is live or position deviation is too big we quickly catch-up, otherwise smoothly correct: */
                    if (timeMSec == endTimeMSec || qAbs(timeMSec - m_animatedPosition) >= qAbs(kCatchUpThresholdMs * speed()))
                        timelineCatchUp(timeMSec);
                    else
                        timelineCorrect(timeMSec);
                }
            }
            else
                /* If media position was not updated at this tick: */
            {
                /* Advance timeline position if it's not already being advanced, corrected or caught-up.
                 * It's easier to handle mode switches here instead of AbstractAnimator::finished signal. */
                if (m_positionAnimator->isStopped())
                    timelineAdvance(timeMSec);
            }
        }

        m_previousMediaPosition = timeMSec;

        NX_TRACE(this, "Updating slider from reader: animated position %1", m_animatedPosition);
        m_timeSlider->setValue(milliseconds(m_animatedPosition), keepInWindow);

        if (timeUSec >= 0)
            updateLive();

        const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
        const bool syncSupportedButDisabled = !streamSynchronizer->isRunning()
            && m_currentWidgetFlags.testFlag(WidgetSupportsSync);

        if (isSearch || syncSupportedButDisabled)
        {
            QVector<milliseconds> indicators;
            for (QnResourceWidget *widget : display()->widgets())
            {
                if (!isSearch && !widget->resource()->hasFlags(Qn::sync))
                    continue;

                QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
                if (!mediaWidget || mediaWidget == m_currentMediaWidget)
                    continue;

                qint64 timeUSec = usecTimeForWidget(mediaWidget);
                indicators.push_back(milliseconds(usecToMsec(timeUSec)));
            }
            m_timeSlider->setIndicators(indicators);
        }
        else
        {
            m_timeSlider->setIndicators(QVector<milliseconds>());
        }
    }

    if (!m_currentWidgetLoaded && widgetLoaded)
    {
        m_currentWidgetLoaded = true;

        if (m_sliderDataInvalid)
        {
            updateSliderFromItemData(m_currentMediaWidget);
            m_timeSlider->finishAnimations();
            m_sliderDataInvalid = false;
            m_sliderWindowInvalid = false;
        }

        if (auto camera = m_currentMediaWidget->resource().dynamicCast<QnVirtualCameraResource>())
        {
            if (camera->isDtsBased())
                updateHasArchive();
        }
        // #spanasenko: A call to updateTimelineRelevancy() has been removed from here
        // because it led to incorrect state of 'LIVE' button for currently recording cameras.
        // Additional research or refactoring may be needed.
    }
}

void QnWorkbenchNavigator::updateCurrentPeriods()
{
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        updateCurrentPeriods(static_cast<Qn::TimePeriodContent>(i));
}

void QnWorkbenchNavigator::updateCurrentPeriods(Qn::TimePeriodContent type)
{
    QnTimePeriodList periods;

    if (type == Qn::MotionContent && m_currentMediaWidget && !m_currentMediaWidget->options().testFlag(QnResourceWidget::DisplayMotion))
    {
        /* Use empty periods. */
    }
    else if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        periods = loader->periods(type);
    }

    if (m_timeSlider)
        m_timeSlider->setTimePeriods(CurrentLine, type, periods);
    if (m_calendar)
        m_calendar->setTimePeriods(type, periods);
}

void QnWorkbenchNavigator::resetSyncedPeriods()
{
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
        auto periodsType = static_cast<Qn::TimePeriodContent>(i);
        if (m_timeSlider)
            m_timeSlider->setTimePeriods(SyncedLine, periodsType, QnTimePeriodList());
        if (m_calendar)
            m_calendar->setAllCamerasTimePeriods(periodsType, QnTimePeriodList());
    }
}

void QnWorkbenchNavigator::updateSyncedPeriods(qint64 startTimeMs)
{
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        updateSyncedPeriods(static_cast<Qn::TimePeriodContent>(i), startTimeMs);
}

void QnWorkbenchNavigator::updateSyncedPeriods(Qn::TimePeriodContent timePeriodType, qint64 startTimeMs)
{
    /* Merge is not required. */
    if (!m_timeSlider)
        return;

    /* Check if no chunks were updated. */
    if (startTimeMs == DATETIME_NOW)
        return;

    if (!ini().enableSyncedChunksForExtraContent && timePeriodType != Qn::RecordingContent)
        return;

    /* We don't want duplicate providers. */
    QSet<core::CachingCameraDataLoaderPtr> loaders;
    for (const auto widget: m_syncedWidgets)
    {
        if (timePeriodType == Qn::MotionContent && !widget->options().testFlag(QnResourceWidget::DisplayMotion))
            continue; /* Ignore it. */

        // Ignore cross-system cameras from offline Systems.
        if (widget->resource()->toResourcePtr()->hasFlags(Qn::fake))
            continue;

        if (auto loader = loaderByWidget(widget))
            loaders.insert(loader);
    }

    std::vector<QnTimePeriodList> periodsList;
    for (auto loader: loaders)
        periodsList.push_back(loader->periods(timePeriodType));

    NX_VERBOSE(this, "Syncing %1 periods from %2 loaders. Total count is %3.",
        timePeriodType,
        loaders.size(),
        std::accumulate(periodsList.cbegin(), periodsList.cend(), 0,
            [](int result, const QnTimePeriodList& list) { return result + list.size(); }));

    QnTimePeriodList syncedPeriods = m_timeSlider->timePeriods(SyncedLine, timePeriodType);

    NX_VERBOSE(this, "Queuing merge of synced periods (%1) with loaded periods, handle %2",
        syncedPeriods.size(),
        m_chunkMergingProcessHandle);

    m_threadedChunksMergeTool[timePeriodType]->queueMerge(
        periodsList,
        syncedPeriods,
        startTimeMs,
        m_chunkMergingProcessHandle);
}

void QnWorkbenchNavigator::updateLines()
{
    if (!m_timeSlider)
        return;

    bool isZoomed = display()->widget(Qn::ZoomedRole) != nullptr;

    if (m_currentWidgetFlags.testFlag(WidgetSupportsPeriods))
    {
        m_timeSlider->setLineVisible(CurrentLine, true);
        m_timeSlider->setLineVisible(SyncedLine, !isZoomed && m_syncedResources.size() > 1);

        m_timeSlider->setLineComment(CurrentLine, nx::utils::elideString(
            m_currentWidget->resource()->getName(), kMaxTimelineCameraNameLength));
    }
    else
    {
        m_timeSlider->setLineVisible(CurrentLine, false);
        m_timeSlider->setLineVisible(SyncedLine, false);
    }

    LayoutResourcePtr currentLayoutResource = workbench()->currentLayoutResource();
    if (currentLayoutResource
        && (currentLayoutResource->isFile() || !currentLayoutResource->localRange().isEmpty()))
    {
        m_timeSlider->setLastMinuteIndicatorVisible(CurrentLine, false);
        m_timeSlider->setLastMinuteIndicatorVisible(SyncedLine, false);
    }
    else
    {
        auto isNormalCamera =
            [](const QnResourceWidget* widget)
            {
                return widget
                    && !widget->resource()->hasFlags(Qn::local)
                    && !widget->resource()->hasFlags(Qn::virtual_camera);
            };

        bool isSearch = workbench()->currentLayout()->isPreviewSearchLayout();
        bool currentIsNormal = isNormalCamera(m_currentWidget);

        bool haveNormal = currentIsNormal;
        if (!haveNormal)
        {
            for (const QnResourceWidget* widget : m_syncedWidgets)
            {
                if (isNormalCamera(widget))
                {
                    haveNormal = true;
                    break;
                }
            }
        }

        m_timeSlider->setLastMinuteIndicatorVisible(CurrentLine, !isSearch && currentIsNormal);
        m_timeSlider->setLastMinuteIndicatorVisible(SyncedLine, !isSearch && haveNormal);
    }
}

void QnWorkbenchNavigator::updateCalendar()
{
    if (m_calendar)
        m_calendar->periodsVisible = m_currentWidgetFlags.testFlag(WidgetSupportsPeriods);
}

void QnWorkbenchNavigator::updateSliderFromScrollBar()
{
    if (m_updatingScrollBarFromSlider)
        return;

    if (!m_timeSlider)
        return;

    QScopedValueRollback<bool> guard(m_updatingSliderFromScrollBar, true);

    m_timeSlider->setWindow(milliseconds(m_timeScrollBar->value()),
        milliseconds(m_timeScrollBar->value() + m_timeScrollBar->pageStep()));
}

void QnWorkbenchNavigator::updateScrollBarFromSlider()
{
    if (m_updatingSliderFromScrollBar)
        return;

    if (!m_timeSlider)
        return;

    {
        QScopedValueRollback<bool> guard(m_updatingScrollBarFromSlider, true);

        milliseconds windowSize = m_timeSlider->windowEnd() - m_timeSlider->windowStart();

        m_timeScrollBar->setRange(m_timeSlider->minimum().count(),
            (m_timeSlider->maximum() - windowSize).count());
        m_timeScrollBar->setValue(m_timeSlider->windowStart().count());
        m_timeScrollBar->setPageStep(windowSize.count());
        m_timeScrollBar->setIndicatorVisible(m_timeSlider->positionMarkerVisible());
        m_timeScrollBar->setIndicatorPosition(m_timeSlider->sliderTimePosition());
    }

    // Evil call at first sight, but it actually needed for smooth scrolling due "special"
    // implementation of scrollbar and stuff.
    if (m_timeScrollBar->isSliderDown())
        updateSliderFromScrollBar();
}

void QnWorkbenchNavigator::updateCalendarFromSlider()
{
    if (!isValid())
        return;

    const auto targetStart = QDateTime::fromMSecsSinceEpoch(
        m_timeSlider->windowTargetStart().count() + m_calendar->displayOffset);
    const auto targetEnd = QDateTime::fromMSecsSinceEpoch(
        m_timeSlider->windowTargetEnd().count() + m_calendar->displayOffset);

    auto selection = m_calendar->selection();

    if (!m_timeSlider->isAnimatingWindowToCertainPosition() || selection.start != targetStart)
    {
        selection.start = QDateTime::fromMSecsSinceEpoch(
            m_timeSlider->windowStart().count() + m_calendar->displayOffset);
    }
    if (!m_timeSlider->isAnimatingWindowToCertainPosition() || selection.end != targetEnd)
    {
        selection.end = QDateTime::fromMSecsSinceEpoch(
            m_timeSlider->windowEnd().count() + m_calendar->displayOffset);
    }

    if (m_calendar->selection() != selection)
    {
        QScopedValueRollback<bool> guard(m_updatingCalendarFromSlider, true);
        m_calendar->selection = selection;
    }
}

void QnWorkbenchNavigator::updateTimeSliderWindowFromCalendar()
{
    if (!isValid() || m_updatingCalendarFromSlider)
        return;

    m_timeSlider->finishAnimations();

    const bool animate = true;
    const bool forceResize = true;

    m_timeSlider->setWindow(
        milliseconds(
            m_calendar->selection().start.toMSecsSinceEpoch() - m_calendar->displayOffset),
        milliseconds(m_calendar->selection().end.toMSecsSinceEpoch() - m_calendar->displayOffset),
        animate,
        forceResize
    );
}

void QnWorkbenchNavigator::updateLive()
{
    bool live = isLive();
    if (m_lastLive == live)
        return;

    NX_VERBOSE(this, "Live changed to %1", live);
    m_lastLive = live;

    if (live)
    {
        setSpeed(1.0);
        setPosition(DATETIME_NOW);
    }

    emit liveChanged();
}

bool QnWorkbenchNavigator::calculateIsLiveSupported() const
{
    if (m_currentWidget && m_currentWidget->resource()->hasFlags(Qn::virtual_camera))
    {
        for (const QnMediaResourcePtr& resource: m_syncedResources.keys())
        {
            if (calculateResourceWidgetFlags(resource->toResourcePtr()) & WidgetSupportsLive)
                return true;
        }
    }

    return m_currentWidgetFlags & WidgetSupportsLive;
}

void QnWorkbenchNavigator::updateLiveSupported()
{
    bool liveSupported = calculateIsLiveSupported();
    if (m_lastLiveSupported == liveSupported)
        return;

    m_lastLiveSupported = liveSupported;

    emit liveSupportedChanged();
}

void QnWorkbenchNavigator::updatePlaying()
{
    bool playing = isPlaying();
    if (playing == m_lastPlaying)
        return;

    m_lastPlaying = playing;

    if (!playing)
        stopPositionAnimations();

    emit playingChanged();
}

void QnWorkbenchNavigator::updatePlayingSupported()
{
    bool playingSupported = isPlayingSupported();
    if (playingSupported == m_lastPlayingSupported)
        return;

    m_lastPlayingSupported = playingSupported;

    emit playingSupportedChanged();
}

void QnWorkbenchNavigator::updateSpeed()
{
    qreal speed = this->speed();
    if (qFuzzyEquals(m_lastSpeed, speed))
        return;

    m_lastSpeed = speed;

    /* Stop animation now. It might be resumed with new speed at the next display update. */
    stopPositionAnimations();

    emit speedChanged();
}

void QnWorkbenchNavigator::updateSpeedRange()
{
    const auto newSpeedRange = speedRange();

    if (newSpeedRange.fuzzyEquals(m_lastSpeedRange))
        return;

    m_lastSpeedRange = newSpeedRange;
    emit speedRangeChanged();
}

bool QnWorkbenchNavigator::calculateTimelineRelevancy() const
{
    // Timeline is not actual if there is no current widget or it is not a playable media.
    if (!isPlayingSupported())
        return false;

    NX_ASSERT(m_currentMediaWidget);
    if (!m_currentMediaWidget)
        return false;

    const auto resource = m_currentMediaWidget->resource()->toResourcePtr();
    NX_ASSERT(resource);
    if (!resource)
        return false;

    if (resource->hasFlags(Qn::fake))
        return false;

    // Timeline is not relevant for desktop cameras.
    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    // Timeline is always actual for local files.
    if (resource->flags().testFlag(Qn::local))
        return true;

    return isRecording()
        || hasArchive()
        || workbench()->currentLayout()->isPreviewSearchLayout();
}

void QnWorkbenchNavigator::updateTimelineRelevancy()
{
    const bool value = calculateTimelineRelevancy();

    if (m_timelineRelevant == value)
        return;

    m_timelineRelevant = value;
    emit timelineRelevancyChanged(value);

    if (!m_timelineRelevant && !qFuzzyIsNull(speed()))
        setLive(true);
}

void QnWorkbenchNavigator::updateSyncIsForced()
{
    // Force sync if there more than 1 channel of recorders on the scene.
    const auto widgets = m_syncedWidgets;
    const auto syncIsForced = std::count_if(widgets.cbegin(), widgets.cend(),
        [](QnMediaResourceWidget* w)
        {
            const auto camera = w->resource().dynamicCast<QnSecurityCamResource>();
            return camera && camera->isDtsBased();
        }) > 1;

    if (m_syncIsForced == syncIsForced)
        return;

    m_syncIsForced = syncIsForced;

    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    if (syncIsForced && !streamSynchronizer->isRunning())
    {
        if (m_currentWidgetFlags.testFlag(WidgetSupportsSync))
            streamSynchronizer->setState(m_currentWidget);
        else
            streamSynchronizer->start(DATETIME_NOW, 1.0);
    }

    emit syncIsForcedChanged();
}

void QnWorkbenchNavigator::updateThumbnailsLoader()
{
    using ThumbnailLoadingManager =
        workbench::timeline::ThumbnailLoadingManager;

    if (!m_timeSlider)
        return;

    auto canLoadThumbnailsForWidget =
        [this](QnMediaResourceWidget* widget)
        {
            if (!widget)
                return false;

            // Thumbnails are disabled for panoramic cameras.
            if (widget->channelLayout()->channelCount() > 1)
                return false;

            // Thumbnails are disabled for a media other than video.
            if (!widget->hasVideo())
                return false;

            if (const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>())
            {
                if (!ResourceAccessManager::hasPermissions(camera, Qn::ViewFootagePermission))
                    return false;

                // Thumbnails are disabled for I/O modules.
                if (camera->isIOModule())
                    return false;

                // Thumbnails are disabled for NVRs due performance issues.
                if (camera->isNvr())
                    return false;

                // Check if the camera has recorded periods.
                const auto loader = loaderByWidget(widget, false);
                if (!loader || loader->periods(Qn::RecordingContent).empty())
                    return false;
            }

            return true;
        };

    if (!canLoadThumbnailsForWidget(m_currentMediaWidget))
    {
        m_timeSlider->setThumbnailLoadingManager({});
    }
    else
    {
        if (!m_timeSlider->thumbnailLoadingManager()
            || m_timeSlider->thumbnailLoadingManager()->resourceWidget() != m_currentMediaWidget)
        {
            m_timeSlider->setThumbnailLoadingManager(
                std::make_unique<ThumbnailLoadingManager>(m_currentMediaWidget, m_timeSlider));
        }

        const auto cameraDataLoader = loaderByWidget(m_currentMediaWidget);
        if (cameraDataLoader->allowedContent().contains(Qn::RecordingContent))
        {
            m_timeSlider->thumbnailLoadingManager()->setRecordedTimePeriods(
                cameraDataLoader->periods(Qn::RecordingContent));
        }
    }
}

void QnWorkbenchNavigator::updateTimeSliderWindowSizePolicy()
{
    if (!m_timeSlider)
        return;

    /* This option should never be cleared in ActiveX mode */
    if (qnRuntime->isAcsMode())
        return;

    m_timeSlider->setOption(QnTimeSlider::PreserveWindowSize, m_timeSlider->isThumbnailsVisible());
}

void QnWorkbenchNavigator::updateAutoPaused()
{
    const bool noActivity = workbenchContext()->instance<QnWorkbenchUserInactivityWatcher>()->state();
    const bool isTourRunning = action(menu::ToggleShowreelModeAction)->isChecked();

    const bool autoPaused = noActivity && !isTourRunning;

    if (autoPaused == m_autoPaused)
        return;

    if (autoPaused)
    {
        /* Collect all playing resources */
        foreach(QnResourceWidget *widget, display()->widgets())
        {
            QnMediaResourceWidget *mediaResourceWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
            if (!mediaResourceWidget)
                continue;

            QnResourceDisplayPtr resourceDisplay = mediaResourceWidget->display();
            if (resourceDisplay->isPaused())
                continue;

            bool isLive = resourceDisplay->archiveReader() && resourceDisplay->archiveReader()->isRealTimeSource();
            resourceDisplay->pause();
            m_autoPausedResourceDisplays.insert(resourceDisplay, isLive);
        }
    }
    else if (m_autoPaused)
    {
        for (QHash<QnResourceDisplayPtr, bool>::iterator itr = m_autoPausedResourceDisplays.begin(); itr != m_autoPausedResourceDisplays.end(); ++itr)
        {
            itr.key()->play();
            if (itr.value() && itr.key()->archiveReader())
                itr.key()->archiveReader()->jumpTo(DATETIME_NOW, 0);
        }

        m_autoPausedResourceDisplays.clear();
    }

    m_autoPaused = autoPaused;
    action(menu::PlayPauseAction)->setEnabled(!m_autoPaused); /* Prevent special UI reaction on space key*/
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnWorkbenchNavigator::eventFilter(QObject *watched, QEvent *event)
{
    if (m_timeSlider && watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneWheel)
    {
        if (m_timeSlider->scene() && m_timeSlider->scene()->sendEvent(m_timeSlider, event))
            return true;
    }
    else if (m_timeSlider && watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneMouseDoubleClick)
    {
        if (!m_ignoreScrollBarDblClick)
            m_timeSlider->setWindow(m_timeSlider->minimum(), m_timeSlider->maximum(), true);

        m_ignoreScrollBarDblClick = false;
    }

    return base_type::eventFilter(watched, event);
}

void QnWorkbenchNavigator::at_timeSlider_customContextMenuRequested(const QPointF& pos,
    const QPoint& /*screenPos*/)
{
    if (qnRuntime->isVideoWallMode())
        return;

    auto manager = this->menu();

    QnTimePeriod selection;
    if (m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());

    milliseconds position = m_timeSlider->timeFromPosition(pos);

    auto parameters = currentParameters(menu::TimelineScope);
    parameters.setArgument(Qn::TimePeriodRole, selection);
    // TODO: #sivanov Move this out into global scope.
    parameters.setArgument(Qn::TimePeriodsRole, m_timeSlider->timePeriods(CurrentLine, Qn::RecordingContent));
    parameters.setArgument(Qn::MergedTimePeriodsRole, m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent));

    const auto watcher = workbenchContext()->instance<QnTimelineBookmarksWatcher>();
    QnCameraBookmarkList bookmarks = watcher->bookmarksAtPosition(position);
    if (!bookmarks.isEmpty())
        parameters.setArgument(Qn::CameraBookmarkRole, bookmarks.first()); // TODO: #dklychkov Implement sub-menus for the case when there're more than 1 bookmark at the position

    QScopedPointer<QMenu> menu(manager->newMenu(
        menu::TimelineScope, mainWindowWidget(), parameters));
    if (menu->isEmpty())
        return;

    /* Add slider-local actions to the menu. */
    bool selectionEditable = m_timeSlider->options().testFlag(QnTimeSlider::SelectionEditable);
    manager->redirectAction(menu.data(), menu::StartTimeSelectionAction, selectionEditable ? m_startSelectionAction : nullptr);
    manager->redirectAction(menu.data(), menu::EndTimeSelectionAction, selectionEditable ? m_endSelectionAction : nullptr);
    manager->redirectAction(menu.data(), menu::ClearTimeSelectionAction, selectionEditable ? m_clearSelectionAction : nullptr);

    /* Run menu. */
    QAction *action = QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());

    /* Process slider-local actions. */
    if (action == m_startSelectionAction)
    {
        m_timeSlider->setSelection(position, position);
        m_timeSlider->setSelectionValid(true);
    }
    else if (action == m_endSelectionAction)
    {
        m_timeSlider->setSelection(qMin(position, m_timeSlider->selectionStart()), qMax(position, m_timeSlider->selectionEnd()));
        m_timeSlider->setSelectionValid(true);
    }
    else if (action == m_clearSelectionAction)
    {
        m_timeSlider->setSelectionValid(false);
    }
    else if (action == this->action(menu::ZoomToTimeSelectionAction))
    {
        if (!m_timeSlider->isSelectionValid())
            return;

        m_timeSlider->setWindow(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd(), true);
    }
}

core::CachingCameraDataLoaderPtr QnWorkbenchNavigator::loaderByWidget(
    const QnMediaResourceWidget* widget,
    bool createIfNotExists) const
{
    if (!widget || !widget->resource())
        return {};

    auto systemContext = widget->systemContext();
    if (!NX_ASSERT(systemContext)
        || !NX_ASSERT(systemContext == widget->resource()->toResourcePtr()->systemContext()))
    {
        return {};
    }

    return systemContext->cameraDataManager()->loader(widget->resource(), createIfNotExists);
}

bool QnWorkbenchNavigator::hasArchiveForCamera(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

    auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return false;

    if (!systemContext->accessController()->hasPermissions(camera, Qn::ViewFootagePermission))
        return false;

    auto footageServers = systemContext->cameraHistoryPool()->getCameraFootageData(camera, true);
    if (footageServers.empty())
        return false;

    if (const auto loader = systemContext->cameraDataManager()->loader(
        camera,
        /*createIfNotExists*/ false))
    {
        if (!loader->periods(Qn::RecordingContent).empty())
            return true;
    }

    if (camera->isDtsBased())
    {
        auto widget = std::find_if(m_syncedWidgets.cbegin(), m_syncedWidgets.cend(),
            [camera](QnMediaResourceWidget* widget)
            {
                return widget->resource() == camera;
            });

        NX_ASSERT(widget != m_syncedWidgets.cend());
        if (widget == m_syncedWidgets.cend())
            return false;

        const auto startTime = (*widget)->display()->archiveReader()->startTime();
        return startTime != DATETIME_NOW;
    }

    return true;
}

void QnWorkbenchNavigator::updatePeriods(
    const QnMediaResourcePtr& resource,
    Qn::TimePeriodContent type,
    qint64 startTimeMs)
{
    NX_VERBOSE(this, "%1 periods are changed for %2 since %3",
        type, resource, startTimeMs);

    const bool isCurrent = m_currentMediaWidget && m_currentMediaWidget->resource() == resource;
    const bool isSynced = m_syncedResources.contains(resource);

    if (isCurrent)
    {
        updateCurrentPeriods(type);
        if (type == Qn::RecordingContent)
            updateThumbnailsLoader();
    }

    if (isSynced)
    {
        updateSyncedPeriods(type, startTimeMs);
        updateHasArchive();
    }

    if (isCurrent || isSynced)
        updatePlaybackMask();
}

QnTimePeriod QnWorkbenchNavigator::timelineWindow() const
{
    return m_timeSlider
        ? QnTimePeriod::fromInterval(m_timeSlider->windowStart(), m_timeSlider->windowEnd())
        : QnTimePeriod();
}

QnTimePeriod QnWorkbenchNavigator::timelineSelection() const
{
    return m_timeSlider && m_timeSlider->isSelectionValid()
        ? QnTimePeriod::fromInterval(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd())
        : QnTimePeriod();
}

void QnWorkbenchNavigator::clearTimelineSelection()
{
    if (m_timeSlider)
        m_timeSlider->setSelectionValid(false);
}

void QnWorkbenchNavigator::reopenPlaybackConnection(const QnVirtualCameraResourceList& cameras)
{
    QSet<QnVirtualCameraResourcePtr> uniqueCameras(cameras.begin(), cameras.end());
    for (auto camera: uniqueCameras)
    {
        auto widgets = display()->widgets(camera);
        for (auto widget: widgets)
        {
            auto reader = getReader(widget);
            if (!reader)
                continue;

            if (reader->isPaused())
                reader->resume();

            auto pauseSignalConnection = std::make_shared<QMetaObject::Connection>();
            *pauseSignalConnection = QObject::connect(reader, &nx::utils::Thread::paused, this,
                [=]
                {
                    QObject::disconnect(*pauseSignalConnection);

                    reader->reopen();
                    reader->resume();
                },
                Qt::DirectConnection); //< Reopen in the reader thread.

            reader->pause();
        }
    }
}

void QnWorkbenchNavigator::at_timeSlider_valueChanged(milliseconds value)
{
    if (!m_currentWidget)
        return;

    qint64 position = value.count();

    bool live = isLive();
    if (live)
        position = qnSyncTime->currentMSecsSinceEpoch();

    if (m_animatedPosition != position)
    {
        stopPositionAnimations();
        m_animatedPosition = position;
    }

    /* Update reader position. */
    if (m_currentMediaWidget && !m_updatingSliderFromReader)
    {
        if (QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader())
        {
            if (live)
            {
                reader->jumpTo(DATETIME_NOW, 0);
            }
            else
            {
                if (m_preciseNextSeek)
                {
                    reader->jumpTo(position * 1000, position * 1000); /* Precise seek. */
                }
                else if (m_timeSlider->isSliderDown())
                {
                    reader->jumpTo(position * 1000, 0);
                }
                else
                {
                    reader->setSkipFramesToTime(position * 1000); /* Precise seek. */
                }
                m_preciseNextSeek = false;
            }
        }

        updateLive();
        emit positionChanged();
    }

    emit timelinePositionChanged();
}

void QnWorkbenchNavigator::at_timeSlider_sliderPressed()
{
    if (!m_currentWidget)
        return;

    if (!isPlaying())
        m_preciseNextSeek = true; /* When paused, use precise seeks on click. */

    if (m_lastPlaying)
        setPlayingTemporary(false);

    m_pausedOverride = true;
}

void QnWorkbenchNavigator::at_timeSlider_sliderReleased()
{
    if (!m_currentWidget)
        return;

    if (m_lastPlaying)
        setPlayingTemporary(true);

    if (isPlaying())
    {
        // Handler must be re-run for precise seeking.
        at_timeSlider_valueChanged(m_timeSlider->value());
        m_pausedOverride = false;
    }
}

void QnWorkbenchNavigator::at_timeSlider_selectionPressed()
{
    if (!m_currentWidget)
        return;

    if (m_lastPlaying)
        setPlayingTemporary(true);

    setPlaying(false);

    m_pausedOverride = true;
}

void QnWorkbenchNavigator::at_timeSlider_thumbnailClicked()
{
    if (!m_currentWidget)
        return;

    // Use precise seek when positioning through thumbnail click.
    m_preciseNextSeek = true;

    // Handler must be re-run for precise seeking.
    at_timeSlider_valueChanged(m_timeSlider->value());
}

void QnWorkbenchNavigator::syncIfOutOfSyncWithLive(QnResourceWidget *widget)
{
    const auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    if (!streamSynchronizer->isRunning())
        return;

    if (!widget->resource()->flags().testFlag(Qn::sync))
        return;

    auto mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if (!mediaWidget)
        return;

    auto reader = mediaWidget->display()->archiveReader();
    if (!reader)
        return;

    const bool outOfSync = reader->isRealTimeSource() &&
        streamSynchronizer->state().timeUs != DATETIME_NOW;

    if (!outOfSync)
        return;

    setPosition(DATETIME_NOW);
}

void QnWorkbenchNavigator::at_display_widgetChanged(Qn::ItemRole role)
{
    if (role == Qn::CentralRole)
        updateCentralWidget();

    if (role == Qn::ZoomedRole)
    {
        // Zoom activates live mode for the camera with no archive.
        // When the camera was not playing video because it has been synced with other camera's archive,
        // it is not going to play live video on its own (no component requests stream synchronizer to do this).
        // So it is requested manually here.
        if (auto widget = display()->widget(Qn::ZoomedRole))
            syncIfOutOfSyncWithLive(widget);

        updateLines();
        updateCalendar();
    }
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget)
{
    if (widget->resource()->flags().testFlag(Qn::sync))
    {
        if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
        {
            addSyncedWidget(mediaWidget);
            connect(mediaWidget, &QnMediaResourceWidget::motionSelectionChanged, this,
                [this, mediaWidget]
                {
                    at_widget_motionSelectionChanged(mediaWidget);
                });

            connect(mediaWidget, &QnMediaResourceWidget::licenseStatusChanged, this,
                &QnWorkbenchNavigator::updateThumbnailsLoader);

            if (!hasArchive())
                updateFootageState();
        }
    }

    connect(widget, &QnResourceWidget::optionsChanged, this,
        [this, widget] { at_widget_optionsChanged(widget); });

    connect(widget->resource().get(), &QnResource::flagsChanged, this,
        &QnWorkbenchNavigator::at_resource_flagsChanged);
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget)
{
    widget->disconnect(this);
    widget->resource()->disconnect(this);

    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
    {
        const auto flags = widget->resource()->flags();
        if (flags.testFlag(Qn::sync))
        {
            removeSyncedWidget(mediaWidget);
            if (hasArchive())
                updateFootageState();
        }
    }
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged(QnMediaResourceWidget *widget)
{
    /* We check that the loader can be created (i.e. that the resource is camera)
     * just to feel safe. */
    if (auto loader = loaderByWidget(widget))
        loader->setMotionSelection(widget->motionSelection());
}

void QnWorkbenchNavigator::at_widget_optionsChanged(QnResourceWidget *widget)
{
    int oldSize = m_motionIgnoreWidgets.size();
    if (widget->options().testFlag(QnResourceWidget::DisplayMotion))
    {
        m_motionIgnoreWidgets.insert(widget);
        if (widget == m_currentMediaWidget)
            at_widget_motionSelectionChanged(m_currentMediaWidget);
    }
    else
    {
        m_motionIgnoreWidgets.remove(widget);
    }
    int newSize = m_motionIgnoreWidgets.size();

    if (oldSize != newSize)
    {
        updateSyncedPeriods(Qn::MotionContent);

        if (widget == m_currentWidget)
            updateCurrentPeriods(Qn::MotionContent);
    }
}

void QnWorkbenchNavigator::at_resource_flagsChanged(const QnResourcePtr& resource)
{
    NX_VERBOSE(this, "Flags are changed for %1", resource);

    if (m_currentWidget && m_currentWidget->resource() == resource)
    {
        updateCurrentWidgetFlags();
        updateTimelineRelevancy();
    }

    updateSyncedPeriods();
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderPressed()
{
    m_lastAdjustTimelineToPosition = m_timeSlider->options().testFlag(QnTimeSlider::AdjustWindowToPosition);
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, false);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderReleased()
{
    // Double-click on slider handle must always unzoom.
    m_ignoreScrollBarDblClick = false;
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, m_lastAdjustTimelineToPosition);
}

bool QnWorkbenchNavigator::hasWidgetWithCamera(const QnSecurityCamResourcePtr& camera) const
{
    return std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(),
        [cameraId = camera->getId()](const QnMediaResourcePtr& resource)
        {
            return resource->toResourcePtr()->getId() == cameraId;
        });
}

void QnWorkbenchNavigator::updateHistoryForCamera(QnSecurityCamResourcePtr camera)
{
    // Silently retry loading for cameras which context is not ready yet.
    if (!camera || (camera->hasFlags(Qn::cross_system) && camera->hasFlags(Qn::fake)))
        return;

    m_updateHistoryQueue.remove(camera);

    auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return;

    if (systemContext->cameraHistoryPool()->isCameraHistoryValid(camera))
        return;

    QnCameraHistoryPool::StartResult result =
        systemContext->cameraHistoryPool()->updateCameraHistoryAsync(camera,
            [this, camera](bool success)
            {
                if (!success)
                    m_updateHistoryQueue.insert(camera); //< retry loading
            });
    if (result == QnCameraHistoryPool::StartResult::failed)
        m_updateHistoryQueue.insert(camera); //< retry loading
}

void QnWorkbenchNavigator::updateSliderBookmarks()
{
    if (!bookmarksModeEnabled())
        return;

    // TODO: #ynikitenkov Finish or ditch. Seems unused now.
    //     m_timeSlider->setBookmarks(m_bookmarkAggregation->bookmarkList());
}

void QnWorkbenchNavigator::updatePlaybackMask()
{
    const auto content = selectedExtraContent();
    QnTimePeriodList playbackMask;

    const bool playbackMaskDisabled = content == Qn::RecordingContent
        || (content == Qn::AnalyticsContent && !ini().enableAnalyticsPlaybackMask);

    if (!playbackMaskDisabled)
    {
        if (ini().enableSyncedChunksForExtraContent)
        {
            playbackMask = m_mergedTimePeriods[content];
        }
        else if (m_currentMediaWidget)
        {
            if (const auto loader = loaderByWidget(m_currentMediaWidget))
                playbackMask = loader->periods(content);
        }
    }

    for (auto widget: m_syncedWidgets)
    {
        if (auto archiveReader = widget->display()->archiveReader())
            archiveReader->setPlaybackMask(playbackMask);
    }

    for (auto widget: display()->widgets())
    {
        const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (!mediaWidget || m_syncedWidgets.contains(mediaWidget))
            continue;

        if (const auto archiveReader = mediaWidget->display()->archiveReader())
            archiveReader->setPlaybackMask({});
    }
}

void QnWorkbenchNavigator::setAnalyticsFilter(const nx::analytics::db::Filter& value)
{
    if (auto loader = loaderByWidget(m_currentMediaWidget))
        loader->setAnalyticsFilter(value);
}

Qn::TimePeriodContent QnWorkbenchNavigator::selectedExtraContent() const
{
    return m_timeSlider ? m_timeSlider->selectedExtraContent() : Qn::RecordingContent;
}

void QnWorkbenchNavigator::setSelectedExtraContent(Qn::TimePeriodContent value)
{
    if (m_timeSlider)
        m_timeSlider->setSelectedExtraContent(value);

    if (auto loader = loaderByWidget(m_currentMediaWidget, /*createIfNotExists*/ true))
    {
        auto allowedContent = loader->allowedContent();
        if (value == Qn::AnalyticsContent)
            allowedContent.insert(Qn::AnalyticsContent);
        else
            allowedContent.erase(Qn::AnalyticsContent);
        loader->setAllowedContent(allowedContent);
    }

    if (value != Qn::RecordingContent)
        updateCurrentPeriods(value);

    updatePlaybackMask();
}
