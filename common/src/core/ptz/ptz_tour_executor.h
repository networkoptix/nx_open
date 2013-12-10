#ifndef QN_PTZ_TOUR_EXECUTOR_H
#define QN_PTZ_TOUR_EXECUTOR_H

#include "ptz_fwd.h"

class QnPtzTourExecutorPrivate;

/**
 * A controller that runs a PTZ tour on a given PTZ controller. Note that it 
 * uses event loop and timers, and thus must be run in a thread with event loop,
 * better an dedicated thread.
 * 
 * Note that public functions of this class are thread-safe. 
 */
class QnPtzTourExecutor: public QObject {
    Q_OBJECT
    typedef QObject base_type;
public:
    QnPtzTourExecutor(const QnPtzControllerPtr &controller);
    virtual ~QnPtzTourExecutor();

    void startTour(const QnPtzTour &tour);
    void stopTour();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

signals:
    void startTourLater(const QnPtzTour &tour);
    void stopTourLater();

private slots:
    void at_controller_synchronized(Qn::PtzDataFields fields);
    void at_startTour_requested(const QnPtzTour &tour);
    void at_stopTour_requested();

private:
    QScopedPointer<QnPtzTourExecutorPrivate> d;
};

#endif // QN_PTZ_TOUR_EXECUTOR_H
