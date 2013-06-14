#ifndef QN_HARDWARE_CHECK_EVENT_FILTER_H
#define QN_HARDWARE_CHECK_EVENT_FILTER_H

#include <QtCore/QObject>

class QGLWidget;

class QnGlHardwareChecker: public QObject {
public:
    QnGlHardwareChecker(QGLWidget *parent);
    virtual ~QnGlHardwareChecker();

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QGLWidget* m_widget;
};

#endif // QN_HARDWARE_CHECK_EVENT_FILTER_H
