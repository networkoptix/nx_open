#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>

#include "recording/time_period.h"
#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QRegion;

class QnWorkbenchDisplay;
class QnTimeSlider;
class QnResourceWidget;
class QnAbstractArchiveReader;
class QnCachingTimePeriodLoader;

class QnWorkbenchNavigator: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchNavigator(QnWorkbenchDisplay *display, QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();

    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);

    QnResourceWidget *currentWidget();

signals:
    void currentWidgetChanged();

protected:
    enum SliderLine {
        CurrentLine,
        SyncedLine,
        SliderLineCount
    };

    void initialize();
    void deinitialize();

    void setCentralWidget(QnResourceWidget *widget);
    void addSyncedWidget(QnResourceWidget *widget);
    void removeSyncedWidget(QnResourceWidget *widget);
    
    void setCurrentWidget(QnResourceWidget *camera);

    QnCachingTimePeriodLoader *loader(const QnResourcePtr &resource);

    Q_SLOT void updateCurrentWidget();
    Q_SLOT void updateSlider();
    void updateToolTipFormat();

    void updateCurrentPeriods();
    void updateCurrentPeriods(Qn::TimePeriodType type);
    void updateSyncedPeriods(Qn::TimePeriodType type);

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
    void at_timeSlider_destroyed();

private:
    QnTimeSlider *m_timeSlider;
    QnWorkbenchDisplay *m_display;

    QSet<QnResourceWidget *> m_syncedWidgets;
    QMultiHash<QnResourcePtr, QHashDummyValue> m_syncedResources;

    QnResourceWidget *m_centralWidget;
    QnResourceWidget *m_currentWidget;
    bool m_currentWidgetIsCamera;

    bool m_wasPlaying;
    bool m_inUpdate;

    QHash<QnResourcePtr, QnCachingTimePeriodLoader *> m_loaderByResource;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
