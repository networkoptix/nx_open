#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <utils/common/longrunnable.h>
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
    enum WidgetFlag {
        WidgetUsesUTC = 0x1,
        WidgetSupportsLive = 0x2,
        WidgetSupportsPeriods = 0x4,
        WidgetSupportsSync = 0x8
    };
    Q_DECLARE_FLAGS(WidgetFlags, WidgetFlag);

    QnWorkbenchNavigator(QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();

    // TODO: move time slider & time scrollbar out.
    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);

    QnTimeScrollBar *timeScrollBar() const;
    void setTimeScrollBar(QnTimeScrollBar *scrollBar);

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

    QnResourceWidget *currentWidget() const;
    WidgetFlags currentWidgetFlags() const;

    Q_SLOT void jumpBackward();
    Q_SLOT void jumpForward();

    Q_SLOT void stepBackward();
    Q_SLOT void stepForward();

    virtual Qn::ActionScope currentScope() const override;
    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void currentWidgetAboutToBeChanged();
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

    void addSyncedWidget(QnResourceWidget *widget);
    void removeSyncedWidget(QnResourceWidget *widget);
    
    SliderUserData currentSliderData() const;
    void setCurrentSliderData(const SliderUserData &localData, bool preferToPreserveWindow = false);

    void setPlayingTemporary(bool playing);

    QnCachingTimePeriodLoader *loader(const QnResourcePtr &resource);
    QnCachingTimePeriodLoader *loader(QnResourceWidget *widget);

    QnThumbnailsLoader *thumbnailLoader(const QnResourcePtr &resource);
    QnThumbnailsLoader *thumbnailLoader(QnResourceWidget *widget);

protected slots:
    void updateCentralWidget();
    void updateCurrentWidget();
    void updateSliderFromReader(bool keepInWindow = true);
    void updateSliderOptions();
    void updateScrollBarFromSlider();
    void updateSliderFromScrollBar();

    void updateCurrentPeriods();
    void updateCurrentPeriods(Qn::TimePeriodRole type);
    void updateSyncedPeriods();
    void updateSyncedPeriods(Qn::TimePeriodRole type);
    void updateTargetPeriod();
    void updateLines();

    void updateLive();
    void updateLiveSupported();
    void updatePlaying();
    void updatePlayingSupported();
    void updateSpeed();
    void updateSpeedRange();
   
    void updateThumbnailsLoader();
    
    void updateCurrentWidgetFlags();

protected slots:
    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionSelectionChanged(QnResourceWidget *widget);
    void at_widget_motionSelectionChanged();
    void at_widget_displayFlagsChanged(QnResourceWidget *widget);
    void at_widget_displayFlagsChanged();

    void at_resource_flagsChanged();
    void at_resource_flagsChanged(const QnResourcePtr &resource);

    void at_loader_periodsChanged(QnCachingTimePeriodLoader *loader, Qn::TimePeriodRole type);
    void at_loader_periodsChanged(Qn::TimePeriodRole type);

    void at_timeSlider_valueChanged(qint64 value);
    void at_timeSlider_sliderPressed();
    void at_timeSlider_sliderReleased();
    void at_timeSlider_selectionPressed();
    void at_timeSlider_selectionChanged();
    void at_timeSlider_customContextMenuRequested(const QPointF &pos, const QPoint &screenPos);
    void at_timeSlider_destroyed();

    void at_timeScrollBar_sliderPressed();
    void at_timeScrollBar_sliderReleased();

    void at_timeScrollBar_destroyed();

private:
    QnTimeSlider *m_timeSlider;
    QnTimeScrollBar *m_timeScrollBar;

    QSet<QnResourceWidget *> m_syncedWidgets;
    QMultiHash<QnResourcePtr, QHashDummyValue> m_syncedResources;

    QSet<QnResourceWidget *> m_motionIgnoreWidgets;

    QnResourceWidget *m_centralWidget;
    QnResourceWidget *m_currentWidget;
    WidgetFlags m_currentWidgetFlags;
    bool m_currentWidgetLoaded;
    bool m_currentWidgetIsCentral;
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

    qreal m_lastSpeed;
    qreal m_lastMinimalSpeed;
    qreal m_lastMaximalSpeed;

    qint64 m_lastUpdateSlider;
    qint64 m_lastCameraTime;

    QAction *m_startSelectionAction, *m_endSelectionAction, *m_clearSelectionAction;

    /** Widget to per-widget slider data mapping. */
    QHash<QnResourceWidget *, SliderUserData> m_localDataByWidget;

    QHash<QnResourcePtr, QnCachingTimePeriodLoader *> m_loaderByResource;
    
    QHash<QnResourcePtr, QnThumbnailsLoader *> m_thumbnailLoaderByResource;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchNavigator::WidgetFlags);

#endif // QN_WORKBENCH_NAVIGATOR_H
