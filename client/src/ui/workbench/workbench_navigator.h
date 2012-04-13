#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QnWorkbenchDisplay;
class QnTimeSlider;
class QnResourceWidget;
class QnAbstractArchiveReader;

class QnWorkbenchNavigator: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchNavigator(QnWorkbenchDisplay *display, QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();

    QnTimeSlider *timeSlider() const;
    void setTimeSlider(QnTimeSlider *timeSlider);


protected:
    void initialize();
    void deinitialize();

private slots:
    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionRegionSelected(const QnResourcePtr &resource, QnAbstractArchiveReader *reader, const QList<QRegion> &selection);

private:
    QnTimeSlider *m_timeSlider;
    QnWorkbenchDisplay *m_display;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
