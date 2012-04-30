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
    bool setLive(bool live);
    bool isLiveSupported() const;

    QnResourceWidget *currentWidget();

    virtual Qn::ActionScope currentScope() const override;
    virtual QVariant currentTarget(Qn::ActionScope scope) const override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void currentWidgetChanged();
    void liveChanged();
    void liveSupportedChanged();

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

    Q_SLOT void updateCurrentWidget();
    Q_SLOT void updateSliderFromReader();
    Q_SLOT void updateScrollBarFromSlider();
    Q_SLOT void updateSliderFromScrollBar();
    void updateToolTipFormat();

    void updateCurrentPeriods();
    void updateCurrentPeriods(Qn::TimePeriodType type);
    void updateSyncedPeriods(Qn::TimePeriodType type);
    void updateLines();

    void updateLive();
    void updateLiveSupported();

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

    bool m_wasPlaying;
    bool m_updatingSliderFromReader;
    bool m_updatingSliderFromScrollBar;
    bool m_updatingScrollBarFromSlider;

    bool m_lastLive;
    bool m_lastLiveSupported;

    QAction *m_clearSelectionAction;

    /** Widget to per-widget slider data mapping. */
    QHash<QnResourceWidget *, SliderUserData> m_localDataByWidget;

    QHash<QnResourcePtr, QnCachingTimePeriodLoader *> m_loaderByResource;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
