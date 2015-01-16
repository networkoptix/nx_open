#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <client/client_globals.h>

#include <ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>
#include <utils/common/long_runnable.h>

class QAction;

class QnWorkbenchDisplay;
class QnTimeSlider;
class QnTimeScrollBar;
class QnResourceWidget;
class QnMediaResourceWidget;
class QnAbstractArchiveReader;
class QnCachingCameraDataLoader;
class QnThumbnailsLoader;
class QnCalendarWidget;
class QnDayTimeWidget;
class QnWorkbenchStreamSynchronizer;
class QnResourceDisplay;
class QnSearchLineEdit;

class QnWorkbenchNavigator: public Connective<QObject>, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT;

    typedef Connective<QObject> base_type;

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

    QnSearchLineEdit *bookmarksSearchWidget() const;
    void setBookmarksSearchWidget(QnSearchLineEdit *bookmarksSearchWidget);

    QnCameraBookmarkTags bookmarkTags() const;
    void setBookmarkTags(const QnCameraBookmarkTags &tags);

    bool isLive() const;
    Q_SLOT bool setLive(bool live);
    bool isLiveSupported() const;

    bool isPlaying() const;
    Q_SLOT bool setPlaying(bool playing);
    bool isPlayingSupported() const;

    qreal speed() const;
    Q_SLOT void setSpeed(qreal speed);
    qreal minimalSpeed() const;
    qreal maximalSpeed() const;

    qint64 position() const;
    void setPosition(qint64 position);

    QnResourceWidget *currentWidget() const;
    WidgetFlags currentWidgetFlags() const;

    Q_SLOT void jumpBackward();
    Q_SLOT void jumpForward();

    Q_SLOT void stepBackward();
    Q_SLOT void stepForward();

    virtual Qn::ActionScope currentScope() const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

    QnCachingCameraDataLoader *loader(const QnResourcePtr &resource);

signals:
    void currentWidgetAboutToBeChanged();
    void currentWidgetChanged();
    void liveChanged();
    void liveSupportedChanged();
    void playingChanged();
    void playingSupportedChanged();
    void speedChanged();
    void speedRangeChanged();
    void positionChanged();

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

    QnCachingCameraDataLoader *loader(QnResourceWidget *widget);

    QnThumbnailsLoader *thumbnailLoader(const QnResourcePtr &resource);
    QnThumbnailsLoader *thumbnailLoader(QnResourceWidget *widget);
public slots:
    void clearLoaderCache();
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
    void updateSyncedPeriods();
    void updateSyncedPeriods(Qn::TimePeriodContent type);
    void updateCurrentBookmarks();
    void updateTargetPeriod();
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

    void updateLoaderPeriods(QnCachingCameraDataLoader *loader, Qn::TimePeriodContent type);
    void updateLoaderBookmarks(QnCachingCameraDataLoader *loader);

    void at_timeSlider_valueChanged(qint64 value);
    void at_timeSlider_sliderPressed();
    void at_timeSlider_sliderReleased();
    void at_timeSlider_selectionPressed();
    void at_timeSlider_selectionReleased();
    void at_timeSlider_customContextMenuRequested(const QPointF &pos, const QPoint &screenPos);
    void updateTimeSliderWindowSizePolicy();
    void at_timeSlider_thumbnailClicked();

    void at_timeScrollBar_sliderPressed();
    void at_timeScrollBar_sliderReleased();
    
    void at_calendar_dateClicked(const QDate &date);

    void at_dayTimeWidget_timeClicked(const QTime &time);

private:
    QnWorkbenchStreamSynchronizer *m_streamSynchronizer;
    QTime m_updateSliderTimer;
    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;
    QnCalendarWidget *m_calendar;
    QnDayTimeWidget *m_dayTimeWidget;
    QnSearchLineEdit *m_bookmarksSearchWidget;

    QSet<QnMediaResourceWidget *> m_syncedWidgets;
    QMultiHash<QnResourcePtr, QHashDummyValue> m_syncedResources;

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

    qint64 m_lastUpdateSlider;
    qint64 m_lastCameraTime;

    QAction *m_startSelectionAction, *m_endSelectionAction, *m_clearSelectionAction;

    QHash<QnResourcePtr, QnCachingCameraDataLoader *> m_loaderByResource;
    
    QHash<QnResourcePtr, QnThumbnailsLoader *> m_thumbnailLoaderByResource;

    QnCameraBookmarkTags m_bookmarkTags;
    QScopedPointer<QCompleter> m_bookmarkTagsCompleter;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchNavigator::WidgetFlags);

#endif // QN_WORKBENCH_NAVIGATOR_H
