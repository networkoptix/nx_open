#ifndef QN_HARDWARE_CHECK_EVENT_FILTER_H
#define QN_HARDWARE_CHECK_EVENT_FILTER_H

#include <QtCore/QObject>

class QGLContext;
class QGLWidget;

class QnGlHardwareChecker: public QObject {
public:
    QnGlHardwareChecker(QGLWidget *parent);
    virtual ~QnGlHardwareChecker();

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
    const QGLContext* m_context;
};

#endif // QN_HARDWARE_CHECK_EVENT_FILTER_H
