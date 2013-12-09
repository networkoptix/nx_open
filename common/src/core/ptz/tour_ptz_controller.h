#ifndef QN_TOUR_PTZ_CONTROLLER_H
#define QN_TOUR_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnTourPtzControllerPrivate;

class QnTourPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnTourPtzController(const QnPtzControllerPtr &baseController);
    virtual ~QnTourPtzController();

    static bool extends(const QnPtzControllerPtr &baseController);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool createTour(const QnPtzTour &tour) override;
    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;
    virtual bool getTours(QnPtzTourList *tours) override;

private:
    bool createTourInternal(QnPtzTour tour);

private:
    QScopedPointer<QnTourPtzControllerPrivate> d;
};


#endif // QN_TOUR_PTZ_CONTROLLER_H
