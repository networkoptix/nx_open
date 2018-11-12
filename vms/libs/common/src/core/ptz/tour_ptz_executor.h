#ifndef QN_TOUR_PTZ_EXECUTOR_H
#define QN_TOUR_PTZ_EXECUTOR_H

#include <QtCore/QObject>

#include "ptz_fwd.h"

class QnTourPtzExecutorPrivate;

/**
 * A controller that runs a PTZ tour on a given PTZ controller. Note that it
 * uses event loop and timers, and thus must be run in a thread with event loop,
 * better an dedicated thread.
 *
 * Public functions of this class are thread-safe.
 */
class QnTourPtzExecutor: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnTourPtzExecutor(const QnPtzControllerPtr &controller, QThreadPool* threadPool);
    virtual ~QnTourPtzExecutor();

    void startTour(const QnPtzTour &tour);
    void stopTour();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    friend class QnTourPtzExecutorPrivate;

    Q_SIGNAL void startTourRequested(const QnPtzTour &tour);
    Q_SIGNAL void stopTourRequested();
    Q_SIGNAL void controllerFinishedLater(Qn::PtzCommand command, const QVariant &data);

    Q_SLOT void at_controller_finished(Qn::PtzCommand command, const QVariant &data);
    Q_SLOT void at_startTourRequested(const QnPtzTour &tour);
    Q_SLOT void at_stopTourRequested();

private:
    QScopedPointer<QnTourPtzExecutorPrivate> d;
};

#endif // QN_TOUR_PTZ_EXECUTOR_H
