#ifndef QN_WORKBENCH_NAVIGATOR_H
#define QN_WORKBENCH_NAVIGATOR_H

#include <QtCore/QObject>

class QnTimeSlider;

class QnWorkbenchNavigator: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchNavigator(QnTimeSlider *timeSlider, QObject *parent = NULL);
    virtual ~QnWorkbenchNavigator();


private:
    QnTimeSlider *m_timeSlider;
};

#endif // QN_WORKBENCH_NAVIGATOR_H
