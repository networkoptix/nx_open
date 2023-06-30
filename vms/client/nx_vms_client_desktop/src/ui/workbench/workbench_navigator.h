// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <analytics/db/analytics_db_types.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <client/client_globals.h>
#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource/data_loaders/camera_data_loader_fwd.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <recording/time_period.h>
#include <ui/common/speed_range.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/threaded_chunks_merge_tool.h>

class QAction;
class QCompleter;

class QnWorkbenchItem;
class QnWorkbenchDisplay;
class QnTimeSlider;
class QnTimeScrollBar;
class QnResourceWidget;
class QnMediaResourceWidget;
class QnAbstractArchiveStreamReader;
class QnSearchQueryStrategy;
class VariantAnimator;

namespace nx { namespace utils { class PendingOperation; }}

namespace nx::vms::client::desktop {

class SystemContext;
class TimelineCalendarWidget;

} // namespace nx::vms::client::desktop

class QnWorkbenchNavigator:
    public QObject,
    public nx::vms::client::desktop::WindowContextAware,
    public nx::vms::client::desktop::menu::TargetProvider
{
    Q_OBJECT;

    typedef QObject base_type;

    using milliseconds = std::chrono::milliseconds;

    Q_PROPERTY(bool hasArchive READ hasArchive NOTIFY hasArchiveChanged)

public:
    enum WidgetFlag
    {
        WidgetUsesUTC = 0x1,
        WidgetSupportsLive = 0x2,
        WidgetSupportsPeriods = 0x4,
        WidgetSupportsSync = 0x8
    };
    Q_DECLARE_FLAGS(WidgetFlags, WidgetFlag);

    explicit QnWorkbenchNavigator(nx::vms::client::desktop::WindowContext* context);
    virtual ~QnWorkbenchNavigator() override;

    // TODO: #sivanov Move time slider & time scrollbar out.
    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);

    QnTimeScrollBar *timeScrollBar() const;
    void setTimeScrollBar(QnTimeScrollBar *scrollBar);

    nx::vms::client::desktop::TimelineCalendarWidget* calendar() const;
    void setCalendar(nx::vms::client::desktop::TimelineCalendarWidget* calendar);

    bool bookmarksModeEnabled() const;
    void setBookmarksModeEnabled(bool bookmarksModeEnabled);

    bool isLive() const;
    Q_SLOT bool setLive(bool live);
    bool isLiveSupported() const;

    bool isPlaying() const;
    Q_SLOT bool setPlaying(bool playing);
    bool isPlayingSupported() const;

    bool hasVmaxInSync() const;
    bool hasVmax() const;

    /** Timeline can be used to navigate through video.  */
    bool isTimelineRelevant() const;

    bool currentWidgetHasVideo() const;

    /** Any of the syncable widgets on the layout has archive. */
    bool hasArchive() const;

    /** Any of the syncable widgets on the layout is recording. */
    bool isRecording() const;

    /** Sync is forcedly enabled. */
    bool syncIsForced() const;
    bool syncEnabled() const;

    qreal speed() const;
    Q_SLOT void setSpeed(qreal speed);
    QnSpeedRange speedRange() const;
    qreal minimalSpeed() const;
    qreal maximalSpeed() const;

    qint64 positionUsec() const;
    void setPosition(qint64 positionUsec);

    QnTimePeriod timelineRange() const;

    QnResourceWidget* currentWidget() const;
    WidgetFlags currentWidgetFlags() const;

    QnMediaResourceWidget* currentMediaWidget() const;

    QnResourcePtr currentResource() const;
    bool currentResourceIsVmax() const;

    bool canJump() const;

    void jumpBackward();
    void jumpForward();

    void stepBackward();
    void stepForward();

    void fastForward();
    void rewind(bool canJumpToPrevious = false);
    void rewindOnDoubleClick();

    virtual nx::vms::client::desktop::menu::ActionScope currentScope() const override;
    virtual nx::vms::client::desktop::menu::Parameters currentParameters(
        nx::vms::client::desktop::menu::ActionScope scope) const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

    // Analytics filter for current media widget.
    void setAnalyticsFilter(const nx::analytics::db::Filter& value);

    Qn::TimePeriodContent selectedExtraContent() const; //< Qn::RecordingContent if none.
    void setSelectedExtraContent(Qn::TimePeriodContent value);

    void setTimelineWindow(const QnTimePeriod& value);
    QnTimePeriod timelineWindow() const;

    void setTimelineSelection(const QnTimePeriod& value);
    QnTimePeriod timelineSelection() const;

    void clearTimelineSelection();

    /** Reopens archive delegates for camera widgets. */
    void reopenPlaybackConnection(const QnVirtualCameraResourceList& cameras);

signals:
    void currentWidgetAboutToBeChanged();
    void currentWidgetChanged();
    void currentResourceChanged();
    void currentLayoutItemRemoved();
    void liveChanged();
    void liveSupportedChanged();
    void hasArchiveChanged();
    void isRecordingChanged();
    void playingChanged();
    void playingSupportedChanged();
    void speedChanged();
    void speedRangeChanged();
    void positionChanged(); //< By seek but not by playback.
    void timelinePositionChanged(); //< By seek or by playback.
    void bookmarksModeEnabledChanged();
    void syncIsForcedChanged();
    void timelineRelevancyChanged(bool isRelevant);
    void timeSelectionChanged(const QnTimePeriod& selection);

protected:
    enum SliderLine
    {
        CurrentLine,
        SyncedLine,
        SliderLineCount
    };

    void initialize();
    void deinitialize();
    bool isValid();

    void addSyncedWidget(QnMediaResourceWidget *widget);
    void removeSyncedWidget(QnMediaResourceWidget *widget);

    void updateItemDataFromSlider(QnResourceWidget *widget) const;
    void updateSliderFromItemData(QnResourceWidget *widget);

    void setPlayingTemporary(bool playing);

protected slots:
    void updateCentralWidget();
    void updateCurrentWidget();
    void updateSliderOptions();
    void updateScrollBarFromSlider();
    void updateSliderFromScrollBar();
    void updateCalendarFromSlider();

    void updateCurrentPeriods();
    void updateCurrentPeriods(Qn::TimePeriodContent type);

    /** Clean synced line. */
    void resetSyncedPeriods();

    /** Update synced line. Empty period means the whole line. Infinite period is not allowed. */
    void updateSyncedPeriods(qint64 startTimeMs = 0);
    void updateSyncedPeriods(Qn::TimePeriodContent timePeriodType, qint64 startTimeMs = 0);

    void updateLines();
    void updateCalendar();

    void updateLive();

    bool calculateIsLiveSupported() const;
    void updateLiveSupported();

    void updatePlaying();
    void updatePlayingSupported();
    void updateSpeed();
    void updateSpeedRange();

    bool calculateTimelineRelevancy() const;
    void updateTimelineRelevancy();
    void updateSyncIsForced();

    void updateLocalOffset();

    void updateThumbnailsLoader();

    WidgetFlags calculateResourceWidgetFlags(const QnResourcePtr& resource) const;
    void updateCurrentWidgetFlags();

    void updateAutoPaused();

    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionSelectionChanged(QnMediaResourceWidget *widget);
    void at_widget_optionsChanged(QnResourceWidget *widget);

    void at_resource_flagsChanged(const QnResourcePtr &resource);

    void updatePeriods(const QnMediaResourcePtr& resource, Qn::TimePeriodContent type,
        qint64 startTimeMs);

    void at_timeSlider_valueChanged(milliseconds value);
    void at_timeSlider_sliderPressed();
    void at_timeSlider_sliderReleased();
    void at_timeSlider_selectionPressed();
    void at_timeSlider_customContextMenuRequested(const QPointF &pos, const QPoint &screenPos);
    void updateTimeSliderWindowSizePolicy();
    void at_timeSlider_thumbnailClicked();

    void at_timeScrollBar_sliderPressed();
    void at_timeScrollBar_sliderReleased();

private:
    void connectToContext(nx::vms::client::core::SystemContext* systemContext);

    enum class UpdateSliderMode
    {
        KeepInWindow,
        ForcedUpdate
    };

    void updateSliderFromReader(UpdateSliderMode mode = UpdateSliderMode::KeepInWindow);

    nx::vms::client::core::CachingCameraDataLoaderPtr loaderByWidget(
        const QnMediaResourceWidget* widget,
        bool createIfNotExists = true) const;

    bool hasArchiveForCamera(const QnSecurityCamResourcePtr& camera) const;
    bool hasWidgetWithCamera(const QnSecurityCamResourcePtr& camera) const;
    void updateHistoryForCamera(QnSecurityCamResourcePtr camera);
    void updateSliderBookmarks();

    void updateFootageState();
    void updateIsRecording(bool forceOn = false);
    void updateHasArchive();

    VariantAnimator* createPositionAnimator();
    void initializePositionAnimations();
    void stopPositionAnimations();

    void timelineAdvance(qint64 fromMs);
    void timelineCorrect(qint64 toMs);
    void timelineCatchUp(qint64 toMs);
    bool isTimelineCatchingUp() const;

    bool isCurrentWidgetSynced() const;

    void updatePlaybackMask();

    void syncIfOutOfSyncWithLive(QnResourceWidget *widget);

    void updateTimeSliderWindowFromCalendar();

private:
    QElapsedTimer m_updateSliderTimer;
    QPointer<QnTimeSlider> m_timeSlider;
    QPointer<QnTimeScrollBar> m_timeScrollBar;
    QPointer<nx::vms::client::desktop::TimelineCalendarWidget> m_calendar;

    QSet<QnMediaResourceWidget *> m_syncedWidgets;
    QHash<QnMediaResourcePtr, int> m_syncedResources;

    QSet<QnResourceWidget *> m_motionIgnoreWidgets;

    QPointer<QnResourceWidget> m_centralWidget;
    QPointer<QnResourceWidget> m_currentWidget;
    QPointer<QnMediaResourceWidget> m_currentMediaWidget;
    WidgetFlags m_currentWidgetFlags = {};
    bool m_currentWidgetLoaded = false;
    bool m_sliderDataInvalid = false;
    bool m_sliderWindowInvalid = false;
    bool m_timelineWindowIsNearLive = false;

    bool m_updatingCalendarFromSlider = false;
    bool m_updatingSliderFromReader = false;
    bool m_updatingSliderFromScrollBar = false;
    bool m_updatingScrollBarFromSlider = false;

    bool m_lastLive = false;
    bool m_lastLiveSupported = false;
    bool m_lastPlaying = false;
    bool m_lastPlayingSupported = false;
    bool m_pausedOverride = false;
    bool m_preciseNextSeek = false;

    bool m_skip1Step = false;

    bool m_ignoreScrollBarDblClick = false;

    /** This flag says that video was paused automatically due to user inactivity.
     *  It's used to make it possible to unpause video only in the user inactivity state handler.
     */
    bool m_autoPaused = false;
    QHash<QnResourceDisplayPtr, bool> m_autoPausedResourceDisplays;

    qreal m_lastSpeed = 0.0;
    QnSpeedRange m_lastSpeedRange;

    bool m_lastAdjustTimelineToPosition = false;

    bool m_timelineRelevant = false;

    QAction *m_startSelectionAction, *m_endSelectionAction, *m_clearSelectionAction;

    QScopedPointer<QCompleter> m_bookmarkTagsCompleter;

    nx::utils::PendingOperation *m_sliderBookmarksRefreshOperation;

    int m_chunkMergingProcessHandle = 0;
    std::array<std::unique_ptr<QnThreadedChunksMergeTool>, Qn::TimePeriodContentCount> m_threadedChunksMergeTool;
    /** Set of cameras, for which history was not loaded and should be updated again. */
    QSet<QnSecurityCamResourcePtr> m_updateHistoryQueue;

    /** Sync is forced for the current set of widgets. */
    bool m_syncIsForced = false;

    /** At least one of the synced widgets has archive. */
    bool m_hasArchive = false;

    /** At least one of the synced widgets is recording. */
    bool m_isRecording = false;

    /** When recording was started, 0 if there's no recording in progress. */
    qint64 m_recordingStartUtcMs = 0;

    /** Animated timeline position. */
    qint64 m_animatedPosition = 0;

    /** Previous media position. */
    qint64 m_previousMediaPosition = 0;

    /** Timeline position animator. */
    QPointer<VariantAnimator> m_positionAnimator;

    nx::utils::ScopedConnections m_currentWidgetConnections;
    nx::utils::ScopedConnections m_centralWidgetConnections;

    std::array<QnTimePeriodList, Qn::TimePeriodContentCount> m_mergedTimePeriods;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchNavigator::WidgetFlags);
