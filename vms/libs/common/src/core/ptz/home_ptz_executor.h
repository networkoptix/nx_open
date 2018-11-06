#ifndef QN_HOME_PTZ_EXECUTOR_H
#define QN_HOME_PTZ_EXECUTOR_H

#include <QtCore/QObject>

#include "ptz_fwd.h"
#include <nx/core/ptz/options.h>

class QnHomePtzExecutorPrivate;

class QnHomePtzExecutor: public QObject {
    Q_OBJECT
    typedef QObject base_type;

public:
    QnHomePtzExecutor(const QnPtzControllerPtr &controller);
    virtual ~QnHomePtzExecutor();

    void restart();
    void stop();
    bool isRunning();

    void setHomePosition(const QnPtzObject &homePosition);

    QnPtzObject homePosition() const;

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    Q_SIGNAL void restartRequested();
    Q_SIGNAL void stopRequested();

    Q_SLOT void at_restartRequested();
    Q_SLOT void at_stopRequested();

private:
    QScopedPointer<QnHomePtzExecutorPrivate> d;
};

#endif // QN_HOME_PTZ_EXECUTOR_H
