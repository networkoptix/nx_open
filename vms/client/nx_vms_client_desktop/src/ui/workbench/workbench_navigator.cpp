#include "workbench_navigator.h"

#include <algorithm>
#include <cassert>
#include <chrono>

#include <QtCore/QTimer>

#include <QtWidgets/QAction>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QApplication>

extern "C"
{
#include <libavutil/avutil.h> // TODO: remove
}

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/storage_resource.h>

#include <camera/cam_display.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_data_manager.h>
#include <camera/client_video_camera.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <camera/resource_display.h>

#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_archive_stream_reader.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/pending_operation.h>

#include <core/resource/avi/avi_resource.h>

#include <server/server_storage_manager.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <nx/utils/string.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include <nx/vms/client/desktop/ini.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "watchers/workbench_user_inactivity_watcher.h"
#include <utils/common/long_runable_cleanup.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_context.h"
#include "workbench_item.h"
#include "workbench_layout.h"

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace std::literals::chrono_literals;

using std::chrono::milliseconds;
using std::chrono::microseconds;

namespace {

const int kCameraHistoryRetryTimeoutMs = 5 * 1000;

const int kDiscardCacheIntervalMs = 60 * 60 * 1000;

/** Size of timeline window near live when there is no recorded periods on cameras. */
const int kTimelineWindowNearLive = 10 * 1000;

const int kMaxTimelineCameraNameLength = 30;

const int kUpdateBookmarksInterval = 2000;

const qreal kCatchUpTimeFactor = 30.0;

const qint64 kCatchUpThresholdMs = 500;
const qint64 kAdvanceIntervalMs = 200;

enum { kMinimalSymbolsCount = 3, kDelayMs = 750 };

QAtomicInt qn_threadedMergeHandle(1);

}

QnWorkbenchNavigator::QnWorkbenchNavigator(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_streamSynchronizer(context()->instance<QnWorkbenchStreamSynchronizer>()),
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

    connect(this, &QnWorkbenchNavigator::currentWidgetChanged, this, &QnWorkbenchNavigator::updateTimelineRelevancy);
    connect(this, &QnWorkbenchNavigator::isRecordingChanged, this, &QnWorkbenchNavigator::updateTimelineRelevancy);
    connect(this, &QnWorkbenchNavigator::hasArchiveChanged, this, &QnWorkbenchNavigator::updateTimelineRelevancy);

    m_cameraDataManager = qnClientModule->cameraDataManager();
    connect(m_cameraDataManager, &QnCameraDataManager::periodsChanged, this,
        &QnWorkbenchNavigator::updateLoaderPeriods);

    connect(qnServerStorageManager, &QnServerStorageManager::serverRebuildArchiveFinished, m_cameraDataManager, &QnCameraDataManager::clearCache);

    // TODO: #GDM Temporary fix for the Feature #4714. Correct change would be: expand getTimePeriods query with Region data,
    // then truncate cached chunks by this region and synchronize the cache.
    QTimer* discardCacheTimer = new QTimer(this);
    discardCacheTimer->setInterval(kDiscardCacheIntervalMs);
    discardCacheTimer->setSingleShot(false);
    connect(discardCacheTimer, &QTimer::timeout, m_cameraDataManager, &QnCameraDataManager::clearCache);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr& res)
    {
        if (res.dynamicCast<QnStorageResource>())
            m_cameraDataManager->clearCache(); // TODO:#GDM #bookmarks check if should be placed into camera manager
    });
    discardCacheTimer->start();

    connect(resourcePool(), &QnResourcePool::statusChanged, this,
        [this](const QnResourcePtr& resource)
        {
            auto cam = resource.dynamicCast<QnSecurityCamResource>();
            if (!m_syncedResources.contains(cam))
                return;

            if (!m_isRecording && resource->getStatus() == Qn::Recording)
                updateIsRecording(true);
            else
                updateFootageState();
        });

    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraHistoryInvalidated, this, [this](const QnSecurityCamResourcePtr &camera)
    {
        if (hasWidgetWithCamera(camera))
            updateHistoryForCamera(camera);
    });

    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraFootageChanged, this, [this](const QnSecurityCamResourcePtr &camera)
    {
        if (auto loader =  m_cameraDataManager->loader(camera))
            loader->discardCachedData();
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

                m_mergedTimePeriods[timePeriodType] = result;

                if (timePeriodType != Qn::RecordingContent && timePeriodType == selectedExtraContent())
                    updatePlaybackMask();

                if (m_timeSlider)
                    m_timeSlider->setTimePeriods(SyncedLine, timePeriodType, result);
                if (m_calendar)
                    m_calendar->setSyncedTimePeriods(timePeriodType, result);
                if (m_dayTimeWidget)
                    m_dayTimeWidget->setSecondaryTimePeriods(timePeriodType, result);
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

    connect(workbench(), &QnWorkbench::layoutChangeProcessStarted, this, [this]
    {
        m_chunkMergingProcessHandle = qn_threadedMergeHandle.fetchAndAddAcquire(1);
    });

    connect(workbench(), &QnWorkbench::layoutChangeProcessFinished, this, [this]
    {
        resetSyncedPeriods();
        updateSyncedPeriods();
    });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource)
    {
        if (QnMediaResourcePtr mediaRes = resource.dynamicCast<QnMediaResource>())
        {
            auto itr = m_thumbnailLoaderByResource.find(mediaRes);
            if (itr != m_thumbnailLoaderByResource.end())
            {
                QnLongRunableCleanup::instance()->cleanupAsync(std::move(itr->second));
                m_thumbnailLoaderByResource.erase(itr);
            }
        }
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
        disconnect(m_timeSlider, NULL, this, NULL);

        if (isValid())
            deinitialize();
    }

    m_timeSlider = timeSlider;

    if (m_timeSlider)
    {
        connect(m_timeSlider, &QObject::destroyed, this, [this]() { setTimeSlider(NULL); });

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

QnCalendarWidget *QnWorkbenchNavigator::calendar() const
{
    return m_calendar;
}

void QnWorkbenchNavigator::setCalendar(QnCalendarWidget *calendar)
{
    if (m_calendar == calendar)
        return;

    if (m_calendar)
    {
        disconnect(m_calendar, NULL, this, NULL);

        if (isValid())
            deinitialize();
    }

    m_calendar = calendar;

    if (m_calendar)
    {
        connect(m_calendar, &QObject::destroyed, this, [this]() { setCalendar(NULL); });

        if (isValid())
            initialize();
    }
}

QnDayTimeWidget *QnWorkbenchNavigator::dayTimeWidget() const
{
    return m_dayTimeWidget;
}

void QnWorkbenchNavigator::setDayTimeWidget(QnDayTimeWidget *dayTimeWidget)
{
    if (m_dayTimeWidget == dayTimeWidget)
        return;

    if (m_dayTimeWidget)
    {
        disconnect(m_dayTimeWidget, NULL, this, NULL);

        if (isValid())
            deinitialize();
    }

    m_dayTimeWidget = dayTimeWidget;

    if (m_dayTimeWidget)
    {
        connect(m_dayTimeWidget, &QObject::destroyed, this, [this]() { setDayTimeWidget(NULL); });

        if (isValid())
            initialize();
    }
}

bool QnWorkbenchNavigator::bookmarksModeEnabled() const
{
    return m_timeSlider->isBookmarksVisible();
}

void QnWorkbenchNavigator::setBookmarksModeEnabled(bool enabled)
{
    if (bookmarksModeEnabled() == enabled)
        return;

    m_timeSlider->setBookmarksVisible(enabled);
    if (enabled)
        qnCameraBookmarksManager->setEnabled(true); //not disabling it anymore
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

    connect(workbench(), &QnWorkbench::currentLayoutChanged,
        this, &QnWorkbenchNavigator::updateSliderOptions);

    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraFootageChanged, this,
        [this](const QnSecurityCamResourcePtr & /* camera */)
        {
            updateFootageState();
        });

    connect(display(), SIGNAL(widgetChanged(Qn::ItemRole)), this, SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(), SIGNAL(widgetAdded(QnResourceWidget *)), this, SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(), SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this, SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display()->beforePaintInstrument(), QnSignalingInstrumentActivated, this,
        [this](){ updateSliderFromReader(); });

    connect(m_timeSlider, &QnTimeSlider::timeChanged,
        this, &QnWorkbenchNavigator::at_timeSlider_valueChanged);
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

    connect(m_calendar, SIGNAL(dateClicked(const QDate &)), this, SLOT(at_calendar_dateClicked(const QDate &)));

    connect(m_dayTimeWidget, SIGNAL(timeClicked(const QTime &)), this, SLOT(at_dayTimeWidget_timeClicked(const QTime &)));

    const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    connect(timeWatcher, &nx::vms::client::core::ServerTimeWatcher::displayOffsetsChanged,
        this, &QnWorkbenchNavigator::updateLocalOffset);

    connect(context()->instance<QnWorkbenchUserInactivityWatcher>(),
        &QnWorkbenchUserInactivityWatcher::stateChanged,
        this,
        &QnWorkbenchNavigator::updateAutoPaused);

    connect(action(action::ToggleLayoutTourModeAction), &QAction::toggled, this,
        &QnWorkbenchNavigator::updateAutoPaused);

    updateLines();
    updateCalendar();
    updateScrollBarFromSlider();
    updateTimeSliderWindowSizePolicy();
    updateAutoPaused();
}

void QnWorkbenchNavigator::deinitialize()
{
    NX_ASSERT(isValid(), "we should definitely be valid here");
    if (!isValid())
        return;

    disconnect(workbench(), NULL, this, NULL);

    disconnect(display(), NULL, this, NULL);
    disconnect(display()->beforePaintInstrument(), NULL, this, NULL);

    disconnect(m_timeSlider, NULL, this, NULL);

    disconnect(m_timeScrollBar, NULL, this, NULL);
    m_timeScrollBar->removeEventFilter(this);

    disconnect(m_calendar, NULL, this, NULL);

    const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    disconnect(timeWatcher, NULL, this, NULL);
    disconnect(qnSettings->notifier(QnClientSettings::TIME_MODE), NULL, this, NULL);

    m_currentWidget = NULL;
    m_currentWidgetFlags = 0;
}

action::ActionScope QnWorkbenchNavigator::currentScope() const
{
    return action::TimelineScope;
}

action::Parameters QnWorkbenchNavigator::currentParameters(action::ActionScope scope) const
{
    if (scope != action::TimelineScope)
        return action::Parameters();

    QnResourceWidgetList result;
    if (m_currentWidget)
        result.push_back(m_currentWidget);
    return action::Parameters(result);
}

bool QnWorkbenchNavigator::isLiveSupported() const
{
    if (m_currentWidget && m_currentWidget->resource()->hasFlags(Qn::wearable_camera))
        for (const QnMediaResourcePtr& resource : m_syncedResources.keys())
            if (calculateResourceWidgetFlags(resource->toResourcePtr()) & WidgetSupportsLive)
                return true;

    return m_currentWidgetFlags & WidgetSupportsLive;
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
            setSpeed(1.0);
    }
    else
    {
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
    bool newValue = accessController()->hasGlobalPermission(GlobalPermission::viewArchive)
        && std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(),
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
    bool newValue = forceOn || (accessController()->hasGlobalPermission(GlobalPermission::viewArchive)
        && std::any_of(m_syncedResources.keyBegin(), m_syncedResources.keyEnd(),
            [](const QnMediaResourcePtr& resource)
            {
                auto camera = resource.dynamicCast<QnSecurityCamResource>();
                return camera && camera->getStatus() == Qn::Recording;
            }));

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

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    if (isPlaying())
        reader->jumpTo(positionUsec, 0);
    else
        reader->jumpTo(positionUsec, positionUsec);
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
    if (widget == NULL)
    {
        qnNullWarning(widget);
        return;
    }

    auto syncedResource = widget->resource();

    m_syncedWidgets.insert(widget);
    ++m_syncedResources[syncedResource];

    connect(syncedResource->toResourcePtr(), &QnResource::parentIdChanged, this, &QnWorkbenchNavigator::updateLocalOffset);

    if(auto loader = m_cameraDataManager->loader(syncedResource))
        loader->setEnabled(true);
    else
        NX_ASSERT(false);

    updateCurrentWidget();
    if (workbench() && !workbench()->isInLayoutChangeProcess())
        updateSyncedPeriods();
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

    disconnect(syncedResource->toResourcePtr(), &QnResource::parentIdChanged, this, &QnWorkbenchNavigator::updateLocalOffset);

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
    m_updateHistoryQueue.remove(widget->resource().dynamicCast<QnSecurityCamResource>());

    if(auto loader = m_cameraDataManager->loader(syncedResource, false))
    {
        loader->setMotionRegions(QList<QRegion>());
        if (noMoreWidgetsOfThisResource)
            loader->setEnabled(false);
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

QnWorkbenchNavigator::WidgetFlags QnWorkbenchNavigator::currentWidgetFlags() const
{
    return m_currentWidgetFlags;
}

void QnWorkbenchNavigator::updateItemDataFromSlider(QnResourceWidget *widget) const
{
    if (!widget || !m_timeSlider)
        return;

    QnWorkbenchItem* item = widget->item();

    QnTimePeriod window(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    if (m_timeSlider->windowEnd() == m_timeSlider->maximum()) // TODO: #Elric check that widget supports live.
        window.durationMs = QnTimePeriod::kInfiniteDuration;
    item->setData(Qn::ItemSliderWindowRole, QVariant::fromValue<QnTimePeriod>(window));

    if (workbench()->currentLayout()->isSearchLayout())
        return;

    QnTimePeriod selection;
    if (m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());
    item->setData(Qn::ItemSliderSelectionRole, QVariant::fromValue<QnTimePeriod>(selection));
}

void QnWorkbenchNavigator::updateSliderFromItemData(QnResourceWidget *widget, bool preferToPreserveWindow)
{
    if (!widget || !m_timeSlider)
        return;

    QnWorkbenchItem *item = widget->item();

    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();
    QnTimePeriod selection = item->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();

    if (preferToPreserveWindow && m_timeSlider->value() >= m_timeSlider->windowStart() && m_timeSlider->value() <= m_timeSlider->windowEnd())
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

    m_timeSlider->setSelectionValid(!selection.isNull());
    m_timeSlider->setSelection(milliseconds(selection.startTimeMs),
        milliseconds(selection.startTimeMs + selection.durationMs));
}


QnThumbnailsLoader *QnWorkbenchNavigator::thumbnailLoader(const QnMediaResourcePtr &resource)
{
    auto pos = m_thumbnailLoaderByResource.find(resource);
    if (pos != m_thumbnailLoaderByResource.end())
        return pos->second.get();

    QnThumbnailsLoader* loader = new QnThumbnailsLoader(resource);
    m_thumbnailLoaderByResource[resource].reset(loader);
    return loader;
}

QnThumbnailsLoader *QnWorkbenchNavigator::thumbnailLoaderByWidget(QnMediaResourceWidget *widget)
{
    return widget ? thumbnailLoader(widget->resource()) : NULL;
}

void QnWorkbenchNavigator::jumpBackward()
{
    if (!m_currentMediaWidget)
        return;

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;

    m_pausedOverride = false;

    qint64 pos = reader->startTime();
    if (auto loader = loaderByWidget(m_currentMediaWidget))
    {
        bool canUseMotion = m_currentWidget->options().testFlag(QnResourceWidget::DisplayMotion);
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() || !canUseMotion ? Qn::RecordingContent : Qn::MotionContent);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        if (!periods.empty())
        {
            if (m_timeSlider->isLive())
            {
                pos = periods.last().startTimeMs * 1000;
            }
            else
            {
                /* We want timeline to jump relatively to current position, not camera frame. */
                qint64 currentTime = m_timeSlider->value().count();

                QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTime, true);
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

    QnAbstractArchiveStreamReader *reader = m_currentMediaWidget->display()->archiveReader();
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
        bool canUseMotion = m_currentWidget->options().testFlag(QnResourceWidget::DisplayMotion);
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() || !canUseMotion ? Qn::RecordingContent : Qn::MotionContent);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION_MS);

        /* We want timeline to jump relatively to current position, not camera frame. */
        qint64 currentTime = m_timeSlider->value().count();

        QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTime, true);
        if (itr != periods.cend() && currentTime >= itr->startTimeMs)
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

        if (reader->isSingleShotMode())
            m_currentMediaWidget->display()->camDisplay()->playAudio(false); // TODO: #Elric wtf?

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
    m_currentMediaWidget->display()->camDisplay()->playAudio(playing);
}

// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void QnWorkbenchNavigator::updateCentralWidget()
{
    // TODO: #GDM get rid of central widget - it is used ONLY as a previous/current value in the layout change process
    QnResourceWidget *centralWidget = display()->widget(Qn::CentralRole);
    if (m_centralWidget == centralWidget)
        return;

    m_centralWidgetConnections = {};

    m_centralWidget = centralWidget;

    if (m_centralWidget)
    {
        m_centralWidgetConnections << connect(m_centralWidget->resource(),
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

    QnMediaResourceWidget* mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);

    const auto previousResource = currentResource();

    emit currentWidgetAboutToBeChanged();

    WidgetFlags previousWidgetFlags = m_currentWidgetFlags;

    m_currentWidgetConnections = {};

    if (m_currentWidget)
    {
        m_timeSlider->setThumbnailsLoader(nullptr, -1);

        if (m_streamSynchronizer->isRunning() && (m_currentWidgetFlags & WidgetSupportsPeriods))
        {
            for (auto widget: m_syncedWidgets)
                updateItemDataFromSlider(widget); // TODO: #GDM #Common ask #elric: should it be done at every selection change?
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
        m_currentWidgetConnections << connect(m_currentWidget,
            &QnMediaResourceWidget::aspectRatioChanged,
            this,
            &QnWorkbenchNavigator::updateThumbnailsLoader);

        m_currentWidgetConnections << connect(m_currentWidget->resource(),
            &QnResource::nameChanged,
            this,
            &QnWorkbenchNavigator::updateLines);
    }

    m_pausedOverride = false;
    m_currentWidgetLoaded = false;

    initializePositionAnimations();

    updateCurrentWidgetFlags();
    updateLines();
    updateCalendar();

    if (!(isCurrentWidgetSynced() && previousWidgetFlags.testFlag(WidgetSupportsSync)) && m_currentWidget)
    {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid |= (m_currentWidgetFlags.testFlag(WidgetUsesUTC)
            != previousWidgetFlags.testFlag(WidgetUsesUTC));
    }

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
            };

        executeDelayedParented(callback, this);
    }

    updateLocalOffset();
    updateCurrentPeriods();
    updateLiveSupported();
    updateLive();
    updatePlayingSupported();
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
    const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    qint64 localOffset = m_currentMediaWidget
        ? timeWatcher->displayOffset(m_currentMediaWidget->resource())
        : 0;

    if (m_timeSlider)
        m_timeSlider->setLocalOffset(milliseconds(localOffset));
    if (m_calendar)
        m_calendar->setLocalOffset(localOffset);
    if (m_dayTimeWidget)
        m_dayTimeWidget->setLocalOffset(localOffset);
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

    if (resource->hasFlags(Qn::wearable_camera))
        result &= ~WidgetSupportsLive;

    return result;
}

void QnWorkbenchNavigator::updateCurrentWidgetFlags()
{
    WidgetFlags flags = 0;

    if (m_currentWidget)
    {
        flags = calculateResourceWidgetFlags(m_currentWidget->resource());

        if (workbench()->currentLayout()->isSearchLayout()) /* Is a thumbnails search layout. */
            flags &= ~(WidgetSupportsLive | WidgetSupportsSync);

        QnTimePeriod period = workbench()->currentLayout()->resource() ? workbench()->currentLayout()->resource()->getLocalRange() : QnTimePeriod();
        if (!period.isNull())
            flags &= ~WidgetSupportsLive;
    }

    if (m_currentWidgetFlags == flags)
        return;

    m_currentWidgetFlags = flags;

    updateSliderOptions();
}

void QnWorkbenchNavigator::updateSliderOptions()
{
    if (!m_timeSlider)
        return;

    m_timeSlider->setOption(QnTimeSlider::UseUTC, m_currentWidgetFlags & WidgetUsesUTC);

    m_timeSlider->setOption(QnTimeSlider::ClearSelectionOnClick,
        !workbench()->currentLayout()->isSearchLayout());

    bool selectionEditable = workbench()->currentLayout()->resource();
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
    animator->setTimer(InstrumentManager::animationTimer(display()->scene()));
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
    return m_positionAnimator->isRunning() &&
        m_positionAnimator->targetValue() == m_previousMediaPosition;
}

bool QnWorkbenchNavigator::isCurrentWidgetSynced() const
{
    return m_streamSynchronizer->isRunning() && m_currentWidgetFlags.testFlag(WidgetSupportsSync);
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

    QnThumbnailsSearchState searchState = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
    bool isSearch = searchState.step > 0;

    qint64 endTimeMSec, startTimeMSec;

    bool widgetLoaded = false;
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

        /* This can also be true if the current widget has no archive at all and all other synced widgets still have not received rtsp response. */
        bool noRecordedPeriodsFound = startTimeUSec == DATETIME_NOW;

        if (!widgetLoaded)
        {
            startTimeMSec = m_timeSlider->minimum().count();
            endTimeMSec = m_timeSlider->maximum().count();
        }
        else if (noRecordedPeriodsFound)
        {
            if (!qnRuntime->isAcsMode())
            {
                if (isRecording()) //< recording has been just started, no archive received yet
                {
                    /* Set to last minute. */
                    endTimeMSec = qnSyncTime->currentMSecsSinceEpoch();
                    startTimeMSec = endTimeMSec - kTimelineWindowNearLive;
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

            if(m_currentWidget->resource()->hasFlags(Qn::wearable_camera))
            {
                bool allWearable = std::all_of(m_syncedWidgets.begin(), m_syncedWidgets.end(),
                    [](QnMediaResourceWidget* widget)
                    {
                        return widget->resource()->toResourcePtr()->hasFlags(Qn::wearable_camera);
                    });

                if (allWearable)
                {
                    QnTimePeriodList periods = m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent);
                    if (!periods.empty() && !periods.back().isInfinite())
                        endTimeMSec = periods.back().endTimeMs();
                }
            }
        }
    }

    if (m_sliderWindowInvalid)
    {
        m_timeSlider->finishAnimations();
        m_timeSlider->invalidateWindow();
    }

    m_timeSlider->setTimeRange(milliseconds(startTimeMSec), milliseconds(endTimeMSec));

    if (m_calendar)
        m_calendar->setDateRange(QDateTime::fromMSecsSinceEpoch(startTimeMSec).date(), QDateTime::fromMSecsSinceEpoch(endTimeMSec).date());
    if (m_dayTimeWidget)
        m_dayTimeWidget->setEnabledWindow(startTimeMSec, endTimeMSec);

    if (!m_currentWidgetLoaded && widgetLoaded && !isSearch
        && display()->widgets().size() == 1
        && m_currentWidget->resource()->hasFlags(Qn::wearable_camera))
    {
        setPosition(startTimeMSec * 1000);
    }

    const auto buffering = widgetLoaded
        && m_currentMediaWidget->display()->camDisplay()->isBuffering();

    if (!m_pausedOverride && !buffering)
    {
        // TODO: #GDM #vkutin #refactor logic in 3.1
        auto usecTimeForWidget = [isSearch, this](QnMediaResourceWidget *mediaWidget) -> qint64
        {
            if (mediaWidget->display()->camDisplay()->isRealTimeSource())
                return DATETIME_NOW;

            qint64 timeUSec;
            if (isCurrentWidgetSynced())
            {
                timeUSec = m_streamSynchronizer->state().timeUs; // Fetch "current" time instead of "displayed"
            }
            else if (mediaWidget->resource()->toResource()->flags().testFlag(Qn::local_media))
            {
                // #vkutin Workaround for local files doing seek after quite a delay
                // which caused time marker to jump back and forth.
                // It happened even before QnCamDisplay::isBuffering became true.
                // So for local files we use position of next frame, it seems working.
                timeUSec = mediaWidget->display()->camDisplay()->getNextTime();
            }
            else
            {
                timeUSec = mediaWidget->display()->camDisplay()->getExternalTime();
            }

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

        /* If in Live we change speed from 1 downwards, we go out of Live, but then
         * receive next frame from camera which immediately jerks us back to Live.
         * This evil hack is to prevent going back to Live in such case: */
        if (timeMSec >= m_timeSlider->maximum().count() && speed() <= 0.0)
            timeMSec = m_timeSlider->maximum().count() - 1;

        if (!keepInWindow || m_sliderDataInvalid)
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

                if (qAbs(delta) < m_timeSlider->msecsPerPixel() ||
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

        m_timeSlider->setValue(milliseconds(m_animatedPosition), keepInWindow);

        if (timeUSec >= 0)
            updateLive();

        const bool syncSupportedButDisabled = !m_streamSynchronizer->isRunning()
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
            updateSliderFromItemData(m_currentMediaWidget, !m_sliderWindowInvalid);
            m_timeSlider->finishAnimations();
            m_sliderDataInvalid = false;
            m_sliderWindowInvalid = false;
        }

        if (auto camera = m_currentMediaWidget->resource().dynamicCast<QnVirtualCameraResource>())
        {
            if (camera->isDtsBased())
                updateHasArchive();
        }
        updateTimelineRelevancy(); //< TODO: #vbreus check if this update really needed
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
        m_calendar->setCurrentTimePeriods(type, periods);
    if (m_dayTimeWidget)
        m_dayTimeWidget->setPrimaryTimePeriods(type, periods);
}

void QnWorkbenchNavigator::resetSyncedPeriods()
{
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
        auto periodsType = static_cast<Qn::TimePeriodContent>(i);
        if (m_timeSlider)
            m_timeSlider->setTimePeriods(SyncedLine, periodsType, QnTimePeriodList());
        if (m_calendar)
            m_calendar->setSyncedTimePeriods(periodsType, QnTimePeriodList());
        if (m_dayTimeWidget)
            m_dayTimeWidget->setSecondaryTimePeriods(periodsType, QnTimePeriodList());
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
    QSet<QnCachingCameraDataLoaderPtr> loaders;
    for (const auto widget: m_syncedWidgets)
    {
        if (timePeriodType == Qn::MotionContent && !widget->options().testFlag(QnResourceWidget::DisplayMotion))
            continue; /* Ignore it. */

        if (auto loader = loaderByWidget(widget))
            loaders.insert(loader);
    }

    std::vector<QnTimePeriodList> periodsList;
    for (auto loader: loaders)
        periodsList.push_back(loader->periods(timePeriodType));

    QnTimePeriodList syncedPeriods = m_timeSlider->timePeriods(SyncedLine, timePeriodType);

    m_threadedChunksMergeTool[timePeriodType]->queueMerge(periodsList, syncedPeriods, startTimeMs, m_chunkMergingProcessHandle);
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

    QnLayoutResourcePtr currentLayoutResource = workbench()->currentLayout()->resource();
    if (currentLayoutResource
        && (currentLayoutResource->isFile() || !currentLayoutResource->getLocalRange().isEmpty()))
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
                    && !widget->resource()->hasFlags(Qn::wearable_camera);
            };

        bool isSearch = workbench()->currentLayout()->isSearchLayout();
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
    if (!m_calendar)
        return;
    if (m_currentWidgetFlags & WidgetSupportsPeriods)
        m_calendar->setCurrentTimePeriodsVisible(true);
    else
        m_calendar->setCurrentTimePeriodsVisible(false);
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

    updateSliderFromScrollBar(); /* Bi-directional sync is needed as time scrollbar may adjust the provided values. */
}

void QnWorkbenchNavigator::updateCalendarFromSlider()
{
    if (!isValid())
        return;

    m_calendar->setSelectedWindow(m_timeSlider->windowStart().count(),
        m_timeSlider->windowEnd().count());
    m_dayTimeWidget->setSelectedWindow(m_timeSlider->windowStart().count(),
        m_timeSlider->windowEnd().count());
}

void QnWorkbenchNavigator::updateLive()
{
    bool live = isLive();
    if (m_lastLive == live)
        return;

    m_lastLive = live;

    if (live)
        setSpeed(1.0);

    emit liveChanged();
}

void QnWorkbenchNavigator::updateLiveSupported()
{
    bool liveSupported = isLiveSupported();
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

    // Timeline is not relevant for desktop cameras.
    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    // Timeline is always actual for local files.
    if (resource->flags().testFlag(Qn::local))
        return true;

    return isRecording()
        || hasArchive()
        || workbench()->currentLayout()->isSearchLayout();
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

    const auto streamSynchronizer = context()->instance<QnWorkbenchStreamSynchronizer>();
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
    if (!m_timeSlider)
        return;

    auto canLoadThumbnailsForWidget =
        [this](QnMediaResourceWidget* widget)
        {
            /* Widget must exist. */
            if (!widget)
                return false;

            /* Widget must have associated resource, supported by thumbnails loader. */
            if (!QnThumbnailsLoader::supportedResource(widget->resource()))
                return false;

            /* First frame is not loaded yet, we must know it for setting up correct aspect ratio. */
            if (!widget->hasAspectRatio())
                return false;

            /* Thumbnails for panoramic cameras are disabled for now. */
            if (widget->channelLayout()->size().width() > 1
                || widget->channelLayout()->size().height() > 1)
                return false;

            /* Thumbnails for I/O modules and sound files are disabled. */
            if (!widget->hasVideo())
                return false;

            if (const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>())
            {
                if (!accessController()->hasPermissions(camera, Qn::ViewFootagePermission))
                    return false;
            }

            /* Further checks must be skipped for local files. */
            QnAviResourcePtr aviFile = widget->resource().dynamicCast<QnAviResource>();
            if (aviFile)
                return true;

            /* Check if the camera has recorded periods. */
            auto loader = loaderByWidget(widget, false);
            if (!loader || loader->periods(Qn::RecordingContent).empty())
                return false;

            return true;
        };

    if (!canLoadThumbnailsForWidget(m_currentMediaWidget))
    {
        m_timeSlider->setThumbnailsLoader(nullptr, -1);
        return;
    }

    m_timeSlider->setThumbnailsLoader(thumbnailLoader(m_currentMediaWidget->resource()), m_currentMediaWidget->aspectRatio());
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
    const bool noActivity = context()->instance<QnWorkbenchUserInactivityWatcher>()->state();
    const bool isTourRunning = action(action::ToggleLayoutTourModeAction)->isChecked();

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
    action(action::PlayPauseAction)->setEnabled(!m_autoPaused); /* Prevent special UI reaction on space key*/
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
    if (!context() || !context()->menu())
    {
        qnWarning("Requesting context menu for a time slider while no menu manager instance is available.");
        return;
    }

    if (qnRuntime->isVideoWallMode())
        return;

    auto manager = context()->menu();

    QnTimePeriod selection;
    if (m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());

    milliseconds position = m_timeSlider->timeFromPosition(pos);

    auto parameters = currentParameters(action::TimelineScope);
    parameters.setArgument(Qn::TimePeriodRole, selection);
    parameters.setArgument(Qn::TimePeriodsRole, m_timeSlider->timePeriods(CurrentLine, Qn::RecordingContent)); // TODO: #Elric move this out into global scope!
    parameters.setArgument(Qn::MergedTimePeriodsRole, m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent));

    const auto watcher = context()->instance<QnTimelineBookmarksWatcher>();
    QnCameraBookmarkList bookmarks = watcher->bookmarksAtPosition(position);
    if (!bookmarks.isEmpty())
        parameters.setArgument(Qn::CameraBookmarkRole, bookmarks.first()); // TODO: #dklychkov Implement sub-menus for the case when there're more than 1 bookmark at the position

    QScopedPointer<QMenu> menu(manager->newMenu(action::TimelineScope, nullptr, parameters));
    if (menu->isEmpty())
        return;

    /* Add slider-local actions to the menu. */
    bool selectionEditable = m_timeSlider->options().testFlag(QnTimeSlider::SelectionEditable);
    manager->redirectAction(menu.data(), action::StartTimeSelectionAction, selectionEditable ? m_startSelectionAction : NULL);
    manager->redirectAction(menu.data(), action::EndTimeSelectionAction, selectionEditable ? m_endSelectionAction : NULL);
    manager->redirectAction(menu.data(), action::ClearTimeSelectionAction, selectionEditable ? m_clearSelectionAction : NULL);

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
    else if (action == context()->action(action::ZoomToTimeSelectionAction))
    {
        if (!m_timeSlider->isSelectionValid())
            return;

        m_timeSlider->setWindow(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd(), true);
    }
}

QnCachingCameraDataLoaderPtr QnWorkbenchNavigator::loaderByWidget(const QnMediaResourceWidget* widget, bool createIfNotExists)
{
    if (!widget || !widget->resource())
        return QnCachingCameraDataLoaderPtr();
    return m_cameraDataManager->loader(widget->resource(), createIfNotExists);
}

QnCameraDataManager* QnWorkbenchNavigator::cameraDataManager() const
{
    return m_cameraDataManager;
}

bool QnWorkbenchNavigator::hasArchiveForCamera(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

    auto footageServers = cameraHistoryPool()->getCameraFootageData(camera, true);
    if (footageServers.empty())
        return false;

    if (const auto loader = m_cameraDataManager->loader(camera, /*createIfNotExists*/ false))
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

void QnWorkbenchNavigator::updateLoaderPeriods(const QnMediaResourcePtr& resource,
    Qn::TimePeriodContent type,
    qint64 startTimeMs)
{
    if (m_currentMediaWidget && m_currentMediaWidget->resource() == resource)
    {
        updateCurrentPeriods(type);
        if (type == Qn::RecordingContent)
            updateThumbnailsLoader();
    }

    if (m_syncedResources.contains(resource))
    {
        updateSyncedPeriods(type, startTimeMs);
        updateHasArchive();
    }

    updatePlaybackMask();
}

void QnWorkbenchNavigator::clearTimeSelection()
{
    if (m_timeSlider)
        m_timeSlider->setSelectionValid(false);
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
    // use precise seek when positioning throuh thumbnail click
    m_preciseNextSeek = true;
}

void QnWorkbenchNavigator::syncIfOutOfSyncWithLive(QnResourceWidget *widget)
{
    if (!m_streamSynchronizer->isRunning())
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
        m_streamSynchronizer->state().timeUs != DATETIME_NOW;

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

    connect(widget, &QnResourceWidget::aspectRatioChanged, this,
        &QnWorkbenchNavigator::updateThumbnailsLoader);
    connect(widget, &QnResourceWidget::optionsChanged, this,
        [this, widget]
        {
            at_widget_optionsChanged(widget);
        });

    connect(widget->resource(), &QnResource::flagsChanged, this,
        &QnWorkbenchNavigator::at_resource_flagsChanged);
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget)
{
    widget->disconnect(this);
    widget->resource()->disconnect(this);

    if (widget->resource()->flags().testFlag(Qn::sync))
    {
        if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
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
        loader->setMotionRegions(widget->motionSelection());
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

void QnWorkbenchNavigator::at_resource_flagsChanged(const QnResourcePtr &resource)
{
    if (!resource || !m_currentWidget || m_currentWidget->resource() != resource)
        return;

    updateCurrentWidgetFlags();
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

void QnWorkbenchNavigator::at_calendar_dateClicked(const QDate &date)
{
    if (!m_timeSlider)
        return;

    QDateTime dateTime(date);
    milliseconds startMSec = milliseconds(dateTime.toMSecsSinceEpoch() - m_calendar->localOffset());
    milliseconds endMSec = milliseconds(dateTime.addDays(1).toMSecsSinceEpoch() - m_calendar->localOffset());

    m_timeSlider->finishAnimations();
    if (QApplication::keyboardModifiers() == Qt::ControlModifier)
    {
        m_timeSlider->setWindow(
            qMin(startMSec, m_timeSlider->windowStart()),
            qMax(endMSec, m_timeSlider->windowEnd()),
            true
        );
    }
    else
    {
        m_timeSlider->setWindow(startMSec, endMSec, true);
    }
}

void QnWorkbenchNavigator::at_dayTimeWidget_timeClicked(const QTime &time)
{
    if (!m_timeSlider || !m_calendar)
        return;

    QDate date = m_calendar->selectedDate();
    if (!date.isValid())
        return;

    QDateTime dateTime = QDateTime(date, time);

    milliseconds startMSec = milliseconds(dateTime.toMSecsSinceEpoch()
        - m_dayTimeWidget->localOffset());
    milliseconds endMSec = milliseconds(dateTime.addSecs(60 * 60).toMSecsSinceEpoch()
        - m_dayTimeWidget->localOffset());

    m_timeSlider->finishAnimations();
    if (QApplication::keyboardModifiers() == Qt::ControlModifier)
    {
        m_timeSlider->setWindow(
            qMin(startMSec, m_timeSlider->windowStart()),
            qMax(endMSec, m_timeSlider->windowEnd()),
            true
        );
    }
    else
    {
        m_timeSlider->setWindow(startMSec, endMSec, true);
    }
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
    if (!camera)
        return;

    m_updateHistoryQueue.remove(camera);

    if (cameraHistoryPool()->isCameraHistoryValid(camera))
        return;

    QnCameraHistoryPool::StartResult result = cameraHistoryPool()->updateCameraHistoryAsync(camera, [this, camera](bool success)
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

    // TODO: #dmishin setting of playback mask for analytics content makes it impossible.
    // to play archive in some cases (if analytics periods are very small)
    // Figure out how to properly fix it.
    if (content != Qn::RecordingContent && content != Qn::AnalyticsContent)
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

    if (value != Qn::RecordingContent)
        updateCurrentPeriods(value);

    updatePlaybackMask();
}
