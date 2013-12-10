#ifndef QN_PTZ_TOUR_EXECUTOR_H
#define QN_PTZ_TOUR_EXECUTOR_H

#include "ptz_fwd.h"

class QnPtzTourExecutorPrivate;

class QnPtzTourExecutor: public QObject {
    Q_OBJECT
    typedef QObject base_type;
public:
    QnPtzTourExecutor(const QnPtzControllerPtr &controller);
    virtual ~QnPtzTourExecutor();

    void startTour(const QnPtzTour &tour);
    void stopTour();

private:
    QScopedPointer<QnPtzTourExecutorPrivate> d;
};

#endif // QN_PTZ_TOUR_EXECUTOR_H