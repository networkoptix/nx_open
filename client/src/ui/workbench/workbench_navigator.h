#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <array>

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>

#include <client/client_globals.h>

#include <recording/time_period.h>

#include <ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>
#include <utils/common/long_runnable.h>
#include <camera/bookmark_queries_cache.h>

class QAction;

class QnWorkbenchItem;
class QnWorkbenchDisplay;
class QnTimeSlider;
class QnTimeScrollBar;
class QnResourceWidget;
class QnMediaResourceWidget;
class QnAbstractArchiveStreamReader;
class QnThumbnailsLoader;
class QnCameraDataManager;
class QnCachingCameraDataLoader;
class QnCalendarWidget;
class QnDayTimeWidget;
class QnWorkbenchStreamSynchronizer;
class QnResourceDisplay;
class QnSearchQueryStrategy;
class QnThreadedChunksMergeTool;
class QnPendingOperation;
class QnCameraBookmarkAggregation;

class QnWorkbenchNavigator: public Connective<QObject>, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT;

    typedef Connective<QObject> base_type;

    Q_PROPERTY(bool hasArchive READ hasArchive NOTIFY hasArchiveChanged)
public:
    enum WidgetFlag {
        WidgetUsesUTC = 0x1,
        WidgetSupportsLive = 0x2,
        WidgetSupportsPeriods = 0x4,
        WidgetSupportsSync = 0x8
    };
    Q_DECLARE_FLAGS(WidgetFlags, WidgetFlag);

    QnWorkbenchNavigator(QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();

    // TODO: #Elric move time slider & time scrollbar out.
    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);

    QnTimeScrollBar *timeScrollBar() const;
    void setTimeScrollBar(QnTimeScrollBar *scrollBar);

    QnCalendarWidget *calendar() const;
    void setCalendar(QnCalendarWidget *calendar);

    QnDayTimeWidget *dayTimeWidget() const;
    void setDayTimeWidget(QnDayTimeWidget *dayTimeWidget);

    bool bookmarksModeEnabled() const;
    void setBookmarksModeEnabled(bool bookmarksModeEnabled);

    bool isLive() const;
    Q_SLOT bool setLive(bool live);
    bool isLiveSupported() const;

    bool isPlaying() const;
    Q_SLOT bool setPlaying(bool playing);
    bool isPlayingSupported() const;

    bool hasVideo() const;

    bool hasArchive() const;

    qreal speed() const;
    Q_SLOT void setSpeed(qreal speed);
    qreal minimalSpeed() const;
    qreal maximalSpeed() const;

    qint64 positionUsec() const;
    void setPosition(qint64 positionUsec);

    QnResourceWidget *currentWidget() const;
    WidgetFlags currentWidgetFlags() const;

    Q_SLOT void jumpBackward();
    Q_SLOT void jumpForward();

    Q_SLOT void stepBackward();
    Q_SLOT void stepForward();

    virtual Qn::ActionScope currentScope() const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void currentWidgetAboutToBeChanged();
    void currentWidgetChanged();
    void liveChanged();
    void liveSupportedChanged();
    void hasArchiveChanged();
    void playingChanged();
    void playingSupportedChanged();
    void speedChanged();
    void speedRangeChanged();
    void positionChanged();
    void bookmarksModeEnabledChanged();

protected:
    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    enum SliderLine {
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
    void updateSliderFromItemData(QnResourceWidget *widget, bool preferToPreserveWindow = false);

    void setPlayingTemporary(bool playing);

    QnThumbnailsLoader *thumbnailLoader(const QnMediaResourcePtr &resource);
    QnThumbnailsLoader *thumbnailLoaderByWidget(QnMediaResourceWidget *widget);

    protected slots:
    void updateCentralWidget();
    void updateCurrentWidget();
    void updateSliderFromReader(bool keepInWindow = true);
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
    void updateLiveSupported();
    void updatePlaying();
    void updatePlayingSupported();
    void updateSpeed();
    void updateSpeedRange();

    void updateLocalOffset();

    Q_SLOT void updateThumbnailsLoader();

    void updateCurrentWidgetFlags();

    void setAutoPaused(bool autoPaused);

protected slots:
    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionSelectionChanged(QnMediaResourceWidget *widget);
    void at_widget_motionSelectionChanged();
    void at_widget_optionsChanged(QnResourceWidget *widget);
    void at_widget_optionsChanged();

    void at_resource_flagsChanged(const QnResourcePtr &resource);

    void updateLoaderPeriods(const QnMediaResourcePtr &resource, Qn::TimePeriodContent type, qint64 startTimeMs);

    void at_timeSlider_valueChanged(qint64 value);
    void at_timeSlider_sliderPressed();
    void at_timeSlider_sliderReleased();
    void at_timeSlider_selectionPressed();
    void at_timeSlider_selectionReleased();
    void at_timeSlider_customContextMenuRequested(const QPointF &pos, const QPoint &screenPos);
    void updateTimeSliderWindowSizePolicy();
    void at_timeSlider_thumbnailClicked();

    void at_bookmarkQuery_bookmarksChanged(const QnCameraBookmarkList &bookmarks
        , bool immediately);

    void at_timeScrollBar_sliderPressed();
    void at_timeScrollBar_sliderReleased();

    void at_calendar_dateClicked(const QDate &date);

    void at_dayTimeWidget_timeClicked(const QTime &time);

private:
    QnCachingCameraDataLoader* loaderByWidget(const QnMediaResourceWidget* widget, bool createIfNotExists = true);

    bool hasWidgetWithCamera(const QnVirtualCameraResourcePtr &camera) const;
    void updateHistoryForCamera(QnVirtualCameraResourcePtr camera);
    void updateSliderBookmarks();

    void updateBookmarksModeState();

    void resetCurrentBookmarkQuery();

    void setCurrentBookmarkQuery(const QnCameraBookmarksQueryPtr &query);

    QnCameraBookmarksQueryPtr refreshQueryFilter(const QnVirtualCameraResourcePtr &camera);

    void onCurrentLayoutChanged();

    void onCurrentLayoutAboutToBeChanged();

    void onItemAdded(QnWorkbenchLayout *layout
        , QnWorkbenchItem *item);

    void onItemRemoved(QnWorkbenchLayout *layout
        , QnWorkbenchItem *item);

    void updateHasArchiveState();

    bool layoutHasAchive();

private:
    QnWorkbenchStreamSynchronizer *m_streamSynchronizer;
    QTime m_updateSliderTimer;
    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;
    QnCalendarWidget *m_calendar;
    QnDayTimeWidget *m_dayTimeWidget;

    QSet<QnMediaResourceWidget *> m_syncedWidgets;
    QMultiHash<QnMediaResourcePtr, QHashDummyValue> m_syncedResources;

    QSet<QnResourceWidget *> m_motionIgnoreWidgets;

    QnResourceWidget *m_centralWidget;
    QnResourceWidget *m_currentWidget;
    QnMediaResourceWidget *m_currentMediaWidget;
    WidgetFlags m_currentWidgetFlags;
    bool m_currentWidgetLoaded;
    bool m_sliderDataInvalid;
    bool m_sliderWindowInvalid;

    bool m_updatingSliderFromReader;
    bool m_updatingSliderFromScrollBar;
    bool m_updatingScrollBarFromSlider;

    bool m_lastLive;
    bool m_lastLiveSupported;
    bool m_lastPlaying;
    bool m_lastPlayingSupported;
    bool m_pausedOverride;
    bool m_preciseNextSeek;

    /** This flag says that video was paused automatically due to user inactivity.
     *  It's used to make it possible to unpause video only in the user inactivity state handler.
     */
    bool m_autoPaused;
    QHash<QSharedPointer<QnResourceDisplay>, bool> m_autoPausedResourceDisplays;

    qreal m_lastSpeed;
    qreal m_lastMinimalSpeed;
    qreal m_lastMaximalSpeed;

    QAction *m_startSelectionAction, *m_endSelectionAction, *m_clearSelectionAction;

    QHash<QnMediaResourcePtr, QnThumbnailsLoader *> m_thumbnailLoaderByResource;

    QScopedPointer<QCompleter> m_bookmarkTagsCompleter;

    QnBookmarkQueriesCache m_bookmarkQueries;
    QnCameraBookmarksQueryPtr m_currentQuery;

    QScopedPointer<QnCameraBookmarkAggregation> m_bookmarkAggregation;
    QnPendingOperation *m_sliderBookmarksRefreshOperation;

    QnCameraDataManager* m_cameraDataManager;

    int m_chunkMergingProcessHandle;
    std::array<QnThreadedChunksMergeTool*, Qn::TimePeriodContentCount> m_threadedChunksMergeTool;
    /** Set of cameras, for which history was not loaded and should be updated again. */
    QSet<QnVirtualCameraResourcePtr> m_updateHistoryQueue;

    bool m_hasArchive;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchNavigator::WidgetFlags);

#endif // QN_WORKBENCH_NAVIGATOR_H
