#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>
#include "workbench.h"

class QnWorkbenchDisplay;
class QnTimeSlider;
class QnResourceWidget;

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
    void at_display_widgetChanged(QnWorkbench::ItemRole role);
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

    void at_widget_motionRegionSelected(const QnResourcePtr &resource, QnAbstractArchiveReader *reader, const QList<QRegion> &selection);

private:
    QnTimeSlider *m_timeSlider;
    QnWorkbenchDisplay *m_display;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
