#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

#include <ui/actions/action_target_provider.h>

#include "recording/time_period.h"
#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QRegion;
class QAction;
class QGraphicsSceneContextMenuEvent;

class QnWorkbenchDisplay;
class QnTimeSlider;
class QnTimeScrollBar;
class QnResourceWidget;
class QnAbstractArchiveReader;
class QnCachingTimePeriodLoader;
class QnThumbnailsLoader;

class QnWorkbenchNavigator: public QObject, public QnWorkbenchContextAware, public QnActionTargetProvider {
    Q_OBJECT;

    typedef QObject base_type;

public:
    QnWorkbenchNavigator(QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();

    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);

    QnTimeScrollBar *timeScrollBar() const;
    void setTimeScrollBar(QnTimeScrollBar *scrollBar);

    bool isLive() const;
    Q_SLOT bool setLive(bool live);
    bool isLiveSupported() const;

    bool isPlaying() const;
    bool setPlaying(bool playing);
    bool isPlayingSupported() const;

    qreal speed() const;
    void setSpeed(qreal speed);
    qreal minimalSpeed() const;
    qreal maximalSpeed() const;

    QnResourceWidget *currentWidget();

    virtual Qn::ActionScope currentScope() const override;
    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void currentWidgetChanged();
    void liveChanged();
    void liveSupportedChanged();
    void playingChanged();
    void playingSupportedChanged();
    void speedChanged();
    void speedRangeChanged();

protected:
    enum SliderLine {
        CurrentLine,
        SyncedLine,
        SliderLineCount
    };

    struct SliderUserData {
        SliderUserData(): window(0, -1), selection(0, 0), selectionValid(false) {}

        QnTimePeriod window;
        QnTimePeriod selection;
        bool selectionValid;
    };

    void initialize();
    void deinitialize();
    bool isValid();

    void setCentralWidget(QnResourceWidget *widget);
    void addSyncedWidget(QnResourceWidget *widget);
    void removeSyncedWidget(QnResourceWidget *widget);
    
    SliderUserData currentSliderData() const;
    void setCurrentSliderData(const SliderUserData &localData);

    QnCachingTimePeriodLoader *loader(const QnResourcePtr &resource);
    QnCachingTimePeriodLoader *loader(QnResourceWidget *widget);

protected slots:
    void updateCurrentWidget();
    void updateSliderFromReader();
    void updateScrollBarFromSlider();
    void delayedLoadThumbnails();
    void loadThumbnails(qint64 startTimeMs, qint64 endTimeMs);
    void updateSliderFromScrollBar();

    void updateCurrentPeriods();
    void updateCurrentPeriods(Qn::TimePeriodType type);
    void updateSyncedPeriods(Qn::TimePeriodType type);
    void updateLines();

    void updateLive();
    void updateLiveSupported();
    void updatePlaying();
    void updatePlayingSupported();
    void updateSpeed();
    void updateSpeedRange();
    void updateThumbnails();

protected slots:
    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionSelectionChanged(QnResourceWidget *widget);
    void at_widget_motionSelectionChanged();

    void at_loader_periodsChanged(QnCachingTimePeriodLoader *loader, Qn::TimePeriodType type);
    void at_loader_periodsChanged(Qn::TimePeriodType type);

    void at_timeSlider_valueChanged(qint64 value);
    void at_timeSlider_sliderPressed();
    void at_timeSlider_sliderReleased();
    void at_timeSlider_contextMenuEvent(QGraphicsSceneContextMenuEvent *event);
    void at_timeSlider_destroyed();

    void at_scrollBar_destroyed();

private:
    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;

    QSet<QnResourceWidget *> m_syncedWidgets;
    QMultiHash<QnResourcePtr, QHashDummyValue> m_syncedResources;

    QnResourceWidget *m_centralWidget;
    QnResourceWidget *m_currentWidget;
    bool m_currentWidgetIsCamera;

    bool m_updatingSliderFromReader;
    bool m_updatingSliderFromScrollBar;
    bool m_updatingScrollBarFromSlider;

    bool m_lastLive;
    bool m_lastLiveSupported;
    bool m_lastPlaying;
    bool m_lastPlayingSupported;

    qreal m_lastSpeed;
    qreal m_lastMinimalSpeed;
    qreal m_lastMaximalSpeed;

    QAction *m_clearSelectionAction;

    /** Widget to per-widget slider data mapping. */
    QHash<QnResourceWidget *, SliderUserData> m_localDataByWidget;

    QHash<QnResourcePtr, QnCachingTimePeriodLoader *> m_loaderByResource;
    
    QScopedPointer<QnThumbnailsLoader> m_thumbnailsLoader;
    qint64 m_thumbnailsStartTimeMs;
    qint64 m_thumbnailsEndTimeMs;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
